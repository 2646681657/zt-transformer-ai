import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile


ROOT = Path(__file__).resolve().parents[1]
NS = {'m': 'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}


class LinkedExportFormulaTests(unittest.TestCase):
    def test_process_inputs_survive_export_as_formula_dependencies(self):
        for number in (1, 2):
            with self.subTest(scheme=number), tempfile.TemporaryDirectory() as tmp:
                source = ROOT / f'tests/fixtures/linked-export-scheme{number}.json'
                spec = json.loads(source.read_text(encoding='utf-8'))
                output = Path(tmp) / 'export.xlsx'
                result = subprocess.run([
                    'powershell.exe', '-NoProfile', '-NonInteractive',
                    '-ExecutionPolicy', 'Bypass', '-File',
                    str(ROOT / 'resources/scripts/export_calculation_sheet.ps1'),
                    '-Template', str(ROOT / 'resources/templates/linked_calculation_sheet.xlsx'),
                    '-Spec', str(source), '-Output', str(output),
                ], capture_output=True, text=True, timeout=60)
                self.assertEqual(result.returncode, 0, result.stderr)
                with zipfile.ZipFile(output) as archive:
                    main = ET.fromstring(archive.read('xl/worksheets/sheet1.xml'))
                    info = ET.fromstring(archive.read('xl/worksheets/exportInfo.xml'))
                    def cell(root, address):
                        return root.find(f'.//m:c[@r="{address}"]', NS)
                    expected = {'B9': 'hvLayerInsulCalcFactor', 'B10': 'hvAxialAllowance_mm',
                                'B11': 'hvDuctLayerRule', 'B12': 'hvMaxTurnsAdjust'}
                    for address, key in expected.items():
                        target = cell(info, address)
                        self.assertIsNotNone(target, f'{key} was lost during export')
                        self.assertEqual(float(target.find('m:v', NS).text), spec['input'][key])
                    for address, parameter in [('X42', '$B$9'), ('AA28', '$B$10'),
                                               ('Z30', '$B$11'), ('Z31', '$B$11'),
                                               ('Z32', '$B$11'), ('Z33', '$B$11'),
                                               ('Z34', '$B$11'), ('V8', '$B$12')]:
                        formula = cell(main, address).find('m:f', NS)
                        self.assertIsNotNone(formula, f'{address} must remain a formula')
                        self.assertIn(parameter, formula.text)
                    self.assertEqual(cell(main, 'R10').find('m:f', NS).text, 'Q35')
                    self.assertIn('P43', cell(main, 'Q35').find('m:f', NS).text)


if __name__ == '__main__':
    unittest.main()
