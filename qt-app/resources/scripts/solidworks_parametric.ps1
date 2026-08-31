param(
    [Parameter(Mandatory = $true)]
    [string]$RequestPath
)

$ErrorActionPreference = "Stop"
$swApp = $null
$model = $null
$drawingModel = $null

function Write-TaskLog {
    param([string]$Message)
    Write-Output ("[SW] " + $Message)
}

function Write-TaskResult {
    param(
        [bool]$Success,
        [string]$Message,
        [string[]]$OutputFiles = @()
    )

    $result = @{
        success = $Success
        message = $Message
        outputFiles = $OutputFiles
    }
    Write-Output ("ZTF_RESULT:" + ($result | ConvertTo-Json -Compress))
}

try {
    if (-not (Test-Path -LiteralPath $RequestPath)) {
        throw "Request file not found: $RequestPath"
    }

    $request = Get-Content -LiteralPath $RequestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $modelTemplatePath = [string]$request.modelTemplatePath
    $drawingTemplatePath = [string]$request.drawingTemplatePath
    $outputDirectory = [string]$request.outputDirectory
    $outputBaseName = [string]$request.outputBaseName

    if (-not (Test-Path -LiteralPath $modelTemplatePath)) {
        throw "Model template not found: $modelTemplatePath"
    }
    if (-not (Test-Path -LiteralPath $drawingTemplatePath)) {
        throw "Drawing template not found: $drawingTemplatePath"
    }
    if ([string]::IsNullOrWhiteSpace($outputBaseName)) {
        throw "Output base name cannot be empty."
    }

    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

    $modelExtension = [IO.Path]::GetExtension($modelTemplatePath).ToLowerInvariant()
    $documentType = switch ($modelExtension) {
        ".sldprt" { 1 }
        ".sldasm" { 2 }
        default { throw "Model template must be a .SLDPRT or .SLDASM file." }
    }

    Write-TaskLog "Connecting to SOLIDWORKS..."
    $comType = [type]::GetTypeFromProgID("SldWorks.Application")
    if ($null -eq $comType) {
        throw "SOLIDWORKS COM API is not registered. Install and activate SOLIDWORKS Desktop first."
    }

    try {
        $swApp = [Runtime.InteropServices.Marshal]::GetActiveObject("SldWorks.Application")
        Write-TaskLog "Connected to the running SOLIDWORKS instance."
    }
    catch {
        $swApp = New-Object -ComObject SldWorks.Application
        Write-TaskLog "Started SOLIDWORKS."
    }
    $swApp.Visible = [bool]$request.solidWorksVisible

    $openErrors = 0
    $openWarnings = 0
    Write-TaskLog "Opening model template..."
    $model = $swApp.OpenDoc6(
        $modelTemplatePath,
        $documentType,
        1,
        "",
        [ref]$openErrors,
        [ref]$openWarnings
    )
    if ($null -eq $model) {
        throw "Failed to open model template. SOLIDWORKS error: $openErrors, warning: $openWarnings"
    }

    foreach ($parameter in $request.parameters) {
        $name = [string]$parameter.name
        $valueMm = [double]$parameter.valueMm
        Write-TaskLog "Setting parameter $name = $valueMm mm"

        $dimension = $model.Parameter($name)
        if ($null -eq $dimension) {
            throw "Dimension not found in model: $name. Use the full name, for example D1@Sketch1."
        }

        # SOLIDWORKS API length values are meters; the UI uses millimeters.
        $dimension.SystemValue = $valueMm / 1000.0
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($dimension)
    }

    Write-TaskLog "Rebuilding model..."
    [void]$model.ForceRebuild3($false)

    $modelOutputPath = Join-Path $outputDirectory ($outputBaseName + $modelExtension)
    $saveErrors = 0
    $saveWarnings = 0
    $saved = $model.Extension.SaveAs(
        $modelOutputPath,
        0,
        1,
        $null,
        [ref]$saveErrors,
        [ref]$saveWarnings
    )
    if (-not $saved) {
        throw "Failed to save parameterized model. SOLIDWORKS error: $saveErrors, warning: $saveWarnings"
    }
    Write-TaskLog "Model saved: $modelOutputPath"

    Write-TaskLog "Creating drawing..."
    $drawingModel = $swApp.NewDocument($drawingTemplatePath, 0, 0.0, 0.0)
    if ($null -eq $drawingModel) {
        throw "Failed to create a drawing from the selected .DRWDOT template."
    }

    $drawing = $drawingModel
    $viewDefinitions = @(
        @{ Name = "*Front";     X = 0.085; Y = 0.195 },
        @{ Name = "*Top";       X = 0.085; Y = 0.075 },
        @{ Name = "*Right";     X = 0.205; Y = 0.195 },
        @{ Name = "*Isometric"; X = 0.205; Y = 0.075 }
    )

    $firstView = $null
    foreach ($definition in $viewDefinitions) {
        $view = $drawing.CreateDrawViewFromModelView3(
            $modelOutputPath,
            [string]$definition.Name,
            [double]$definition.X,
            [double]$definition.Y,
            0.0
        )
        if ($null -eq $view) {
            throw "Failed to create drawing view: $($definition.Name)"
        }
        if ($null -eq $firstView) {
            $firstView = $view
        }
    }

    if ([bool]$request.insertModelDimensions -and $null -ne $firstView) {
        try {
            [void]$drawing.ActivateView($firstView.Name)
            [void]$drawing.InsertModelDimensions(0)
            Write-TaskLog "Model dimensions import completed."
        }
        catch {
            Write-TaskLog "Model dimension import was skipped; dimensions can be arranged manually."
        }
    }

    [void]$drawingModel.ViewZoomtofit2()

    $drawingOutputPath = Join-Path $outputDirectory ($outputBaseName + ".slddrw")
    $drawingSaveErrors = 0
    $drawingSaveWarnings = 0
    $drawingSaved = $drawingModel.Extension.SaveAs(
        $drawingOutputPath,
        0,
        1,
        $null,
        [ref]$drawingSaveErrors,
        [ref]$drawingSaveWarnings
    )
    if (-not $drawingSaved) {
        throw "Failed to save drawing. SOLIDWORKS error: $drawingSaveErrors, warning: $drawingSaveWarnings"
    }
    Write-TaskLog "Drawing saved: $drawingOutputPath"

    $outputFiles = @($modelOutputPath, $drawingOutputPath)
    if ([bool]$request.exportPdf) {
        $pdfOutputPath = Join-Path $outputDirectory ($outputBaseName + ".pdf")
        $pdfErrors = 0
        $pdfWarnings = 0
        $pdfSaved = $drawingModel.Extension.SaveAs(
            $pdfOutputPath,
            0,
            1,
            $null,
            [ref]$pdfErrors,
            [ref]$pdfWarnings
        )
        if (-not $pdfSaved) {
            throw "Failed to export PDF. SOLIDWORKS error: $pdfErrors, warning: $pdfWarnings"
        }
        $outputFiles += $pdfOutputPath
        Write-TaskLog "PDF saved: $pdfOutputPath"
    }

    Write-TaskResult -Success $true -Message "SOLIDWORKS parametric drawing completed." -OutputFiles $outputFiles
    exit 0
}
catch {
    $message = $_.Exception.Message
    Write-TaskLog ("Failed: " + $message)
    Write-TaskResult -Success $false -Message $message
    exit 1
}
finally {
    if ($null -ne $drawingModel) {
        try { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($drawingModel) } catch {}
    }
    if ($null -ne $model) {
        try { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($model) } catch {}
    }
    if ($null -ne $swApp) {
        try { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($swApp) } catch {}
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
