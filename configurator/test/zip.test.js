'use strict';
const test = require('node:test');
const assert = require('node:assert');
const Z = require('../zip.js');

test('crc32 of a known string', () => {
  assert.strictEqual(Z.crc32(new TextEncoder().encode('123456789')), 0xcbf43926);
});

test('the archive lists every file, stored, with the right sizes', () => {
  const files = [
    { path: 'a/config.h', text: 'int a;\n' },
    { path: 'b.yaml', text: 'name: "é"\n' },
  ];
  const z = Z.zip(files, new Date(2026, 9, 2, 12, 0, 0));
  const dv = new DataView(z.buffer);
  const end = z.length - 22;
  assert.strictEqual(dv.getUint32(end, true), 0x06054b50);
  assert.strictEqual(dv.getUint16(end + 10, true), 2);
  let p = dv.getUint32(end + 16, true);
  for (const f of files) {
    assert.strictEqual(dv.getUint32(p, true), 0x02014b50);
    const size = dv.getUint32(p + 24, true);
    const nameLen = dv.getUint16(p + 28, true);
    const local = dv.getUint32(p + 42, true);
    assert.strictEqual(dv.getUint32(local, true), 0x04034b50);
    assert.strictEqual(new TextDecoder().decode(z.subarray(p + 46, p + 46 + nameLen)), f.path);
    const data = z.subarray(local + 30 + nameLen, local + 30 + nameLen + size);
    assert.strictEqual(new TextDecoder().decode(data), f.text);
    assert.strictEqual(dv.getUint32(p + 16, true), Z.crc32(data));
    p += 46 + nameLen;
  }
});
