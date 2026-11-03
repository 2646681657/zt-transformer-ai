param([string]$Template,[string]$Spec,[string]$Output)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$specData=Get-Content -LiteralPath $Spec -Raw -Encoding UTF8 | ConvertFrom-Json
Copy-Item -LiteralPath $Template -Destination $Output
(Get-Item -LiteralPath $Output).IsReadOnly=$false
$zip=[IO.Compression.ZipFile]::Open($Output,[IO.Compression.ZipArchiveMode]::Update)
function Read-Part($name) { $stream=$zip.GetEntry($name).Open();$r=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8);try{return $r.ReadToEnd()}finally{$r.Dispose()} }
function Write-Part($name,$text) { $old=$zip.GetEntry($name);if($old){$old.Delete()};$entry=$zip.CreateEntry($name);$writer=[IO.StreamWriter]::new($entry.Open(),[Text.UTF8Encoding]::new($false));try{$writer.Write($text)}finally{$writer.Dispose()} }
$ns='http://schemas.openxmlformats.org/spreadsheetml/2006/main'
function Set-Cell($doc,$address,$value,$allowFormula=$false) {
 $manager=[Xml.XmlNamespaceManager]::new($doc.NameTable);$manager.AddNamespace('m',$ns)
 $cell=$doc.SelectSingleNode("//m:c[@r='$address']",$manager)
 if(!$cell){throw "Template cell missing: $address"}
 if($cell.SelectSingleNode('m:f',$manager) -and !$allowFormula){return}
 while($cell.HasChildNodes){[void]$cell.RemoveChild($cell.FirstChild)}
 [void]$cell.RemoveAttribute('t')
 $number=0.0
 if([double]::TryParse([string]$value,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$number)) {
  $v=$doc.CreateElement('v',$ns);$v.InnerText=$number.ToString('R',[Globalization.CultureInfo]::InvariantCulture);[void]$cell.AppendChild($v)
 } else {
  $cell.SetAttribute('t','inlineStr');$is=$doc.CreateElement('is',$ns);$t=$doc.CreateElement('t',$ns);$t.InnerText=[string]$value;[void]$is.AppendChild($t);[void]$cell.AppendChild($is)
 }
}
try {
 [xml]$sheet=Read-Part 'xl/worksheets/sheet1.xml'
 foreach($p in $specData.cells.psobject.Properties){Set-Cell $sheet $p.Name $p.Value}
 if($specData.manualCurrent){Set-Cell $sheet 'O9' $specData.currentStandard $true}
 if($specData.wavePitch -ne 41){Set-Cell $sheet 'S47' $specData.wavePitch $true}
 $tapManager=[Xml.XmlNamespaceManager]::new($sheet.NameTable);$tapManager.AddNamespace('m',$ns)
 $sheet.SelectSingleNode("//m:c[@r='V5']/m:f",$tapManager).InnerText='Y5*(1+L3*N3)'
 $sheet.SelectSingleNode("//m:c[@r='AA5']/m:f",$tapManager).InnerText="Y5*(1-'导出说明'!`$B`$8*N3)"
 if($specData.customInsulation){
  $mgr=[Xml.XmlNamespaceManager]::new($sheet.NameTable);$mgr.AddNamespace('m',$ns)
  foreach($address in @('X14','Z14')){$f=$sheet.SelectSingleNode("//m:c[@r='$address']/m:f",$mgr);$base=if($address -eq 'X14'){'X13'}else{'Z13'};$f.InnerText='IF(U14="自定义",'+$base+"+'导出说明'!`$B`$7,"+$f.InnerText+')'}
 }
 Write-Part 'xl/worksheets/sheet1.xml' $sheet.OuterXml
 [xml]$options=Read-Part 'xl/worksheets/sheet3.xml'
 foreach($p in $specData.controls.psobject.Properties){Set-Cell $options $p.Name $p.Value}
 if($specData.customInsulation){
  $mgr=[Xml.XmlNamespaceManager]::new($options.NameTable);$mgr.AddNamespace('m',$ns)
  $row=$options.SelectSingleNode("//m:row[@r='9']",$mgr);$c=$options.SelectSingleNode("//m:c[@r='W9']",$mgr)
  if(!$c){$c=$options.CreateElement('c',$ns);$c.SetAttribute('r','W9');[void]$row.AppendChild($c)}
  Set-Cell $options 'W9' '自定义' $true
 }
 foreach($row in $options.SelectNodes("//*[local-name()='row']")) {
  $ordered=@($row.ChildNodes | Sort-Object { $index=0;foreach($letter in ($_.GetAttribute('r') -replace '[0-9]','').ToCharArray()){$index=$index*26+[int]$letter-64};$index })
  foreach($cell in $ordered){[void]$row.AppendChild($cell)}
 }
 Write-Part 'xl/worksheets/sheet3.xml' $options.OuterXml
 foreach($name in @($zip.Entries | Where-Object {$_.FullName -like 'xl/ctrlProps/*.xml'} | ForEach-Object {$_.FullName})) {
  [xml]$control=Read-Part $name;$address=($control.DocumentElement.GetAttribute('fmlaLink') -split '!')[-1].Replace('$','')
  $selection=$specData.controls.psobject.Properties[$address]
  if($address -eq 'X2' -and $specData.customInsulation){$control.DocumentElement.SetAttribute('fmlaRange',('线圈型式!`$W`$2:`$W`$9').Replace('`',''))}
  if($selection){$control.DocumentElement.SetAttribute('sel',[string]$selection.Value);Write-Part $name $control.OuterXml}
 }
 foreach($name in @($zip.Entries | Where-Object {$_.FullName -like 'xl/drawings/*.vml'} | ForEach-Object {$_.FullName})) {
  [xml]$vml=Read-Part $name
  foreach($data in $vml.SelectNodes("//*[local-name()='ClientData']")) {
   $link=$data.SelectSingleNode("*[local-name()='FmlaLink']")
   if(!$link){continue};$address=($link.InnerText -split '!')[-1].Replace('$','')
   $selection=$specData.controls.psobject.Properties[$address]
   if($selection){$sel=$data.SelectSingleNode("*[local-name()='Sel']");if($sel){$sel.InnerText=[string]$selection.Value}}
  }
  if($specData.customInsulation){foreach($range in $vml.SelectNodes("//*[local-name()='FmlaRange']")){if($range.InnerText -match 'W\$?2:.*W\$?8'){$range.InnerText=$range.InnerText -replace 'W(\$?)8','W${1}9'}}}
  Write-Part $name $vml.OuterXml
 }
 # Add a human-readable comparison sheet without replacing calculation formulas.
 [xml]$info='<?xml version="1.0" encoding="utf-8"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><cols><col min="1" max="1" width="32" customWidth="1"/><col min="2" max="2" width="100" customWidth="1"/></cols><sheetData/></worksheet>'
 $rows=@(@('导出说明','主表使用原计算单公式，可修改下拉和输入。程序结果在本表供核对。'),@('查表数据说明',$specData.dataNote))
 foreach($p in $specData.engine.psobject.Properties){$rows+=,@($p.Name,[string]$p.Value)}
 $rows+=,@('自定义绝缘总增厚(mm)',[string]$specData.insulationIncrement)
 $rows+=,@('负分接档数',[string]$specData.tapMinusSteps)
 $i=0
 foreach($pair in $rows){$i++;$row=$info.CreateElement('row',$ns);$row.SetAttribute('r',[string]$i);[void]$info.DocumentElement.LastChild.AppendChild($row);for($j=0;$j -lt 2;$j++){$cell=$info.CreateElement('c',$ns);$cell.SetAttribute('r',([string][char](65+$j))+$i);$cell.SetAttribute('t','inlineStr');$is=$info.CreateElement('is',$ns);$text=$info.CreateElement('t',$ns);$text.InnerText=[string]$pair[$j];[void]$is.AppendChild($text);[void]$cell.AppendChild($is);[void]$row.AppendChild($cell)}}
 Write-Part 'xl/worksheets/exportInfo.xml' $info.OuterXml
 [xml]$book=Read-Part 'xl/workbook.xml';$new=$book.CreateElement('sheet',$ns);$new.SetAttribute('name','导出说明');$new.SetAttribute('sheetId','30');$attr=$book.CreateAttribute('r','id','http://schemas.openxmlformats.org/officeDocument/2006/relationships');$attr.Value='rIdExportInfo';[void]$new.Attributes.Append($attr);[void]$book.DocumentElement.SelectSingleNode("*[local-name()='sheets']").AppendChild($new);Write-Part 'xl/workbook.xml' $book.OuterXml
 [xml]$rels=Read-Part 'xl/_rels/workbook.xml.rels';$new=$rels.CreateElement('Relationship',$rels.DocumentElement.NamespaceURI);$new.SetAttribute('Id','rIdExportInfo');$new.SetAttribute('Type','http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet');$new.SetAttribute('Target','worksheets/exportInfo.xml');[void]$rels.DocumentElement.AppendChild($new);Write-Part 'xl/_rels/workbook.xml.rels' $rels.OuterXml
 [xml]$types=Read-Part '[Content_Types].xml';$new=$types.CreateElement('Override',$types.DocumentElement.NamespaceURI);$new.SetAttribute('PartName','/xl/worksheets/exportInfo.xml');$new.SetAttribute('ContentType','application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml');[void]$types.DocumentElement.AppendChild($new);$jsonType=$types.CreateElement('Default',$types.DocumentElement.NamespaceURI);$jsonType.SetAttribute('Extension','json');$jsonType.SetAttribute('ContentType','application/json');[void]$types.DocumentElement.AppendChild($jsonType);Write-Part '[Content_Types].xml' $types.OuterXml
 # Keep the input snapshot and engine values as explicit export evidence, not replacement formulas.
 Write-Part 'ztbld-export.json' ($specData | ConvertTo-Json -Depth 30)
} finally {$zip.Dispose()}
