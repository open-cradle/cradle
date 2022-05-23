import argparse
import pathlib
from re import M
import xml.etree.ElementTree as ET
import json

parser = argparse.ArgumentParser(
    description='Convert Catch XML benchmark results to Google JSON format.')
parser.add_argument('xml_file', type=pathlib.Path)
args = parser.parse_args()

google = {"benchmarks": []}

xml = ET.parse(args.xml_file)
results = xml.getroot().findall('./Group/TestCase/BenchmarkResults')
for result in results:
    mean = result.find('./mean')
    google['benchmarks'].append({
        'name': result.attrib['name'],
        'iterations': int(result.attrib['iterations']),
        'real_time': float(mean.attrib['value']),
        'cpu_time': float(mean.attrib['value']),
        'time_unit': 'ns'
    })

json_file = args.xml_file.with_suffix('.json')
with json_file.open('w') as f:
    json.dump(google, f, indent=4)
