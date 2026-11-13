/*
 * PVRouter configurator - a minimal ZIP writer (stored, no compression), for "download all".
 * zip([{ path, text }]) -> Uint8Array
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.PVRZip = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const CRC_TABLE = (() => {
    const table = new Uint32Array(256);
    for (let n = 0; n < 256; ++n) {
      let c = n;
      for (let k = 0; k < 8; ++k) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
      table[n] = c >>> 0;
    }
    return table;
  })();

  function crc32(bytes) {
    let c = 0xffffffff;
    for (const b of bytes) c = CRC_TABLE[(c ^ b) & 0xff] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
  }

  function zip(files, date) {
    const d = date || new Date();
    const dosTime = (d.getHours() << 11) | (d.getMinutes() << 5) | (d.getSeconds() >> 1);
    const dosDate = ((d.getFullYear() - 1980) << 9) | ((d.getMonth() + 1) << 5) | d.getDate();
    const enc = new TextEncoder();
    const locals = [];
    const centrals = [];
    let offset = 0;

    for (const f of files) {
      const name = enc.encode(f.path);
      const data = enc.encode(f.text);
      const crc = crc32(data);
      // flag bit 11: names are UTF-8
      const common = [
        [2, 20], // version needed
        [2, 0x0800], // flags
        [2, 0], // stored
        [2, dosTime],
        [2, dosDate],
        [4, crc],
        [4, data.length],
        [4, data.length],
        [2, name.length],
        [2, 0], // extra length
      ];
      const local = pack([[4, 0x04034b50], ...common]);
      locals.push(local, name, data);
      centrals.push(pack([[4, 0x02014b50], [2, 20], ...common, [2, 0], [2, 0], [2, 0], [4, 0], [4, offset]]), name);
      offset += local.length + name.length + data.length;
    }
    const centralSize = centrals.reduce((s, a) => s + a.length, 0);
    const end = pack([[4, 0x06054b50], [2, 0], [2, 0], [2, files.length], [2, files.length], [4, centralSize], [4, offset], [2, 0]]);
    return concat([...locals, ...centrals, end]);
  }

  // little-endian fields: [[size in bytes, value], ...]
  function pack(fields) {
    const out = new Uint8Array(fields.reduce((s, [n]) => s + n, 0));
    let i = 0;
    for (const [n, v] of fields) for (let k = 0; k < n; ++k) out[i++] = (v >>> (8 * k)) & 0xff;
    return out;
  }

  function concat(parts) {
    const out = new Uint8Array(parts.reduce((s, a) => s + a.length, 0));
    let i = 0;
    for (const p of parts) {
      out.set(p, i);
      i += p.length;
    }
    return out;
  }

  return { zip, crc32 };
});
