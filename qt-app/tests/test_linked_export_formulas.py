import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / 'resources/templates/linked_calculation_sheet.xlsx'
NS = {'m': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}
# These historical selectors must have no effect, even when malformed.
TUNING_KEYS = (
    'coreWidthOffsets_mm', 'coreSheetCountAdjust', 'designAreaMode',
    'unitIronLossMode', 'centerDistanceAllowance_mm', 'hvLayerInsulCalcFactor',
    'hvMaxTurnsAdjust', 'hvAxialAllowance_mm', 'hvDuctLayerRule',
    'impedanceOutputMode', 'impedanceCalibrationFactor', 'activePartHvWeightMode',
    'thermalFormulaMode', 'lvThermalExtraMode', 'tankRimHighCapacity_mm',
    'tankWallThicknessOverride_mm', 'tankBottomThicknessAdjust_mm',
    'waveLongCountAdjust', 'oilTopCorrection_K',
)


def cell(sheet, address):
    return sheet.find(f'.//m:c[@r="{address}"]', NS)


def formula(sheet, address):
    return cell(sheet, address).find('m:f', NS).text


def export(spec, directory, name='export'):
    source = directory / f'{name}.json'
    output = directory / f'{name}.xlsx'
    source.write_text(json.dumps(spec, ensure_ascii=False), encoding='utf-8')
    result = subprocess.run([
        'powershell.exe', '-NoProfile', '-NonInteractive',
        '-ExecutionPolicy', 'Bypass', '-File',
        str(ROOT / 'resources/scripts/export_calculation_sheet.ps1'),
        '-Template', str(TEMPLATE), '-Spec', str(source), '-Output', str(output),
    ], capture_output=True, text=True, errors='replace', timeout=60)
    if result.returncode:
        raise AssertionError(result.stderr)
    return output


def fixture(number):
    return json.loads((ROOT / f'tests/fixtures/linked-export-scheme{number}.json')
                      .read_text(encoding='utf-8'))


class LinkedExportFormulaTests(unittest.TestCase):
    def test_default_formulas_preserve_real_scheme_inputs(self):
        with zipfile.ZipFile(TEMPLATE) as archive:
            template = ET.fromstring(archive.read('xl/worksheets/sheet1.xml'))
            unchanged_parts = {name: archive.read(name) for name in (
                'xl/worksheets/sheet2.xml', 'xl/worksheets/sheet4.xml',
                'xl/worksheets/sheet5.xml', 'xl/worksheets/sheet6.xml', 'xl/styles.xml')}
        for number in (1, 2):
            with self.subTest(scheme=number), tempfile.TemporaryDirectory() as tmp:
                spec = fixture(number)
                # A stale caller may still supply the removed oil correction.
                spec['cells']['L51'] = '9'
                output = export(spec, Path(tmp))
                with zipfile.ZipFile(output) as archive:
                    main = ET.fromstring(archive.read('xl/worksheets/sheet1.xml'))
                    info = ET.fromstring(archive.read('xl/worksheets/exportInfo.xml'))
                    for address in ('B9', 'B10', 'B11', 'B12'):
                        self.assertIsNone(cell(info, address), 'Removed process tuning row')
                    changed = {'V5', 'AA5'}
                    if spec['customInsulation']:
                        changed.update(('X14', 'Z14'))
                    if spec['manualCurrent']:
                        changed.add('O9')
                    if spec['wavePitch'] != 41:
                        changed.add('S47')
                    for target in template.findall('.//m:c[m:f]', NS):
                        address = target.attrib['r']
                        if address in changed:
                            continue
                        self.assertEqual(formula(main, address), formula(template, address), address)
                    self.assertEqual(formula(main, 'X42'), 'CEILING(2*Y9*AC4/(AC9*4900*X35),1)')
                    self.assertEqual(formula(main, 'AA28'), 'ROUND(AA26*1.01/5,0)*5+1')
                    self.assertEqual(formula(main, 'V8'), 'ROUND(V6/AC4,0)')
                    self.assertEqual(formula(main, 'R10'), 'Q35')
                    self.assertIn('P43', formula(main, 'Q35'))
                    self.assertEqual(float(cell(main, 'L51').find('m:v', NS).text), 6)
                    for address in ('H12', 'K12', 'M12', 'X13', 'Z13', 'AF14',
                                    'AJ14', 'W12', 'AH8', 'X35', 'F24', 'H25',
                                    'J23', 'J26', 'AG12', 'AJ12'):
                        self.assertEqual(float(cell(main, address).find('m:v', NS).text),
                                         float(spec['cells'][address]), address)
                    self.assertEqual(formula(main, 'V5'), 'Y5*(1+L3*N3)')
                    self.assertIn('$B$8', formula(main, 'AA5'))
                    if number == 2:
                        self.assertEqual(float(cell(main, 'S47').find('m:v', NS).text),
                                         spec['wavePitch'])
                    if spec['customInsulation']:
                        self.assertIn('$B$7', formula(main, 'X14'))
                    for name, content in unchanged_parts.items():
                        self.assertEqual(archive.read(name), content, name)
                    book = ET.fromstring(archive.read('xl/workbook.xml'))
                    self.assertEqual(book.find('m:calcPr', NS).get('fullCalcOnLoad'), '1')
                    for name in archive.namelist():
                        if name.startswith('xl/worksheets/') and name.endswith('.xml'):
                            sheet = ET.fromstring(archive.read(name))
                            self.assertFalse(sheet.findall('.//m:c[@t="e"]', NS), name)
                    self.assertFalse(any(n.startswith('xl/externalLinks/') for n in archive.namelist()))

    def test_legacy_formula_tuning_cannot_change_exported_calculation(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            for number in (1, 2):
                original = fixture(number)
                variants = [original, copy.deepcopy(original), copy.deepcopy(original)]
                for key in TUNING_KEYS:
                    variants[1]['input'].pop(key, None)
                    variants[2]['input'][key] = 'obsolete-invalid-value'
                parts = []
                for index, spec in enumerate(variants):
                    output = export(spec, directory, f'scheme{number}-{index}')
                    with zipfile.ZipFile(output) as archive:
                        # Snapshots intentionally retain historical provenance; calculation must ignore it.
                        parts.append({n: archive.read(n) for n in archive.namelist()
                                      if n.startswith('xl/worksheets/') and n.endswith('.xml')})
                with self.subTest(scheme=number):
                    self.assertEqual(parts[0], parts[1])
                    self.assertEqual(parts[0], parts[2])

    @unittest.skipUnless(os.environ.get('LINKED_EXPORT_EXCEL_QA') == '1',
                         'Set LINKED_EXPORT_EXCEL_QA=1 for independent Excel recalculation')
    def test_excel_recalculation_has_no_errors_or_hashes_and_remains_linked(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            outputs = [export(fixture(n), directory, f'scheme{n}') for n in (1, 2)]
            script = directory / 'recalculate.ps1'
            script.write_text(r'''
param([string]$First,[string]$Second)
$ErrorActionPreference='Stop'
$excel=New-Object -ComObject Excel.Application
$excel.Visible=$false
$excel.DisplayAlerts=$false
$excel.AskToUpdateLinks=$false
try {
 foreach($path in @($First,$Second)) {
  $book=$excel.Workbooks.Open($path,0,$false)
  try {
   $sheet=$book.Worksheets.Item(1)
   $excel.CalculateFullRebuild()
   $area=$sheet.Range('F14').Value2
   foreach($pass in @(0,1)) {
    if($pass -eq 1){$sheet.Range('H12').Value2=$sheet.Range('H12').Value2+5;$excel.CalculateFullRebuild();if($sheet.Range('F14').Value2 -eq $area){throw 'Core dimension no longer linked'}}
    foreach($c in $sheet.Range('A1:AL55').Cells) {
     if($c.Text -match '^#+$'){throw ('Hash display at '+$c.Address())}
     if($excel.WorksheetFunction.IsError($c)){throw ('Formula error at '+$c.Address()+': '+$c.Text)}
    }
    if($sheet.Range('R10').Value2 -ne $sheet.Range('Q35').Value2){throw 'Impedance must use Q35 directly'}
   }
  } finally {$book.Close($false)}
 }
 Write-Output 'Both schemes recalculated and linked: no formula errors or hash displays'
} finally {$excel.Quit();[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($excel)}
''', encoding='utf-8-sig')
            result = subprocess.run([
                'powershell.exe', '-NoProfile', '-NonInteractive', '-ExecutionPolicy',
                'Bypass', '-File', str(script), '-First', str(outputs[0]),
                '-Second', str(outputs[1]),
            ], capture_output=True, text=True, errors='replace', timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
