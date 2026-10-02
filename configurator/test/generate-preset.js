// Usage: node generate-preset.js <preset.json> <output dir>
// Writes a copy of the router (and of the receiver for each remote unit) with the files
// generated from the preset, ready for 'pio run'. Exits with 1 if the page would refuse it.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const M = require('../model.js');
const V = require('../validate.js');
const G = require('../generate.js');

const [presetFile, outDir] = process.argv.slice(2);
const repo = path.join(__dirname, '..', '..');
const m = M.normalize(JSON.parse(fs.readFileSync(presetFile, 'utf8')));

const problems = V.validate(m);
for (const p of problems) console.log(`${p.level}: ${p.key} ${JSON.stringify(p.params)}`);
if (V.hasErrors(problems)) process.exit(1);

const skip = (src) => !src.split(path.sep).includes('.pio');
fs.cpSync(path.join(repo, 'Mk2_3phase_RFdatalog_temp'), path.join(outDir, 'Mk2_3phase_RFdatalog_temp'), { recursive: true, filter: skip });
for (let unit = 1; unit <= M.remoteUnitsUsed(m); ++unit)
  fs.cpSync(path.join(repo, 'RemoteLoadReceiver'), path.join(outDir, `RemoteLoadReceiver-unit${unit}`), { recursive: true, filter: skip });

for (const f of G.files(m)) {
  fs.mkdirSync(path.dirname(path.join(outDir, f.path)), { recursive: true });
  fs.writeFileSync(path.join(outDir, f.path), f.text);
  console.log(`wrote ${f.path}`);
}
