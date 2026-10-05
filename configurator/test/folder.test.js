// Saving into the firmware folder, on an in-memory folder that behaves like the browser's handles.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const F = require('../folder.js');

// getFile() gives a Blob-like object; write() takes a string or such an object
class FileHandle {
  constructor(name, content) {
    this.kind = 'file';
    this.name = name;
    this.content = content;
  }
  async getFile() {
    const content = this.content;
    return { content, text: async () => content };
  }
  async createWritable() {
    let data;
    return { write: async (d) => (data = typeof d === 'string' ? d : d.content), close: async () => (this.content = data) };
  }
}

class DirHandle {
  constructor(name, tree = {}) {
    this.kind = 'directory';
    this.name = name;
    this.items = new Map(Object.entries(tree).map(([k, v]) => [k, typeof v === 'string' ? new FileHandle(k, v) : new DirHandle(k, v)]));
  }
  async get(name, kind, create, make) {
    let h = this.items.get(name);
    if (!h && create) this.items.set(name, (h = make()));
    if (!h) throw new Error('NotFoundError');
    if (h.kind !== kind) throw new Error('TypeMismatchError');
    return h;
  }
  getDirectoryHandle(name, { create } = {}) {
    return this.get(name, 'directory', create, () => new DirHandle(name));
  }
  getFileHandle(name, { create } = {}) {
    return this.get(name, 'file', create, () => new FileHandle(name, ''));
  }
  async removeEntry(name, { recursive } = {}) {
    const h = this.items.get(name);
    if (!h) throw new Error('NotFoundError');
    if (h.kind === 'directory' && h.items.size && !recursive) throw new Error('InvalidModificationError');
    this.items.delete(name);
  }
  async *entries() {
    yield* [...this.items.entries()];
  }
}

// the folder as { path: content }, for whole-tree comparisons
function flat(dir, prefix = '') {
  const out = {};
  for (const [name, h] of dir.items) {
    if (h.kind === 'file') out[prefix + name] = h.content;
    else Object.assign(out, flat(h, `${prefix}${name}/`));
  }
  return out;
}

const firmware = () =>
  new DirHandle('PVRouter-3-phase-main', {
    'Mk2_3phase_RFdatalog_temp': { 'Mk2_3phase_RFdatalog_temp.ino': 'ino', 'config.h': 'old', 'calibration.h': 'mine' },
    RemoteLoadReceiver: { 'RemoteLoadReceiver.ino': 'rx ino', 'receiver.cpp': 'rx', 'config.h': 'rx old', '.pio': { build: 'x' } },
  });

const FILES = [
  { path: 'Mk2_3phase_RFdatalog_temp/config.h', text: 'new' },
  { path: 'Mk2_3phase_RFdatalog_temp/config_system.h', text: 'system' },
  { path: 'RemoteLoadReceiver-unit1/config.h', text: 'unit 1' },
  { path: 'RemoteLoadReceiver-unit1/config_rf.h', text: 'unit 1 rf' },
  { path: 'mk2pvrouter.yaml', text: 'yaml' },
];

const saveAll = async (root, files, stamp) => {
  const p = await F.prepare(root, files);
  assert.ok(!p.error, p.error);
  return { p, backup: await F.save(p, stamp) };
};

test('the extracted firmware folder: everything is saved, calibration.h is kept', async () => {
  const root = firmware();
  const { p } = await saveAll(root, FILES, '2026-10-05_10-00-00');
  assert.deepStrictEqual(p.skip, []);
  const all = flat(root);
  assert.strictEqual(all['Mk2_3phase_RFdatalog_temp/config.h'], 'new');
  assert.strictEqual(all['Mk2_3phase_RFdatalog_temp/config_system.h'], 'system');
  assert.strictEqual(all['Mk2_3phase_RFdatalog_temp/calibration.h'], 'mine');
  assert.strictEqual(all['RemoteLoadReceiver-unit1/RemoteLoadReceiver-unit1.ino'], 'rx ino');
  assert.strictEqual(all['RemoteLoadReceiver-unit1/receiver.cpp'], 'rx');
  assert.strictEqual(all['RemoteLoadReceiver-unit1/config.h'], 'unit 1');
  assert.strictEqual(all['RemoteLoadReceiver-unit1/config_rf.h'], 'unit 1 rf');
  assert.ok(!('RemoteLoadReceiver-unit1/RemoteLoadReceiver.ino' in all));
  assert.ok(!Object.keys(all).some((k) => k.startsWith('RemoteLoadReceiver-unit1/.pio')));
  assert.strictEqual(all['RemoteLoadReceiver/config.h'], 'rx old');
  assert.strictEqual(all['mk2pvrouter.yaml'], 'yaml');
});

test('the backup holds what was replaced, and the manifest what was created', async () => {
  const root = firmware();
  const { backup } = await saveAll(root, FILES, '2026-10-05_10-00-00');
  assert.strictEqual(backup, 'configurator-backup/2026-10-05_10-00-00');
  const all = flat(root);
  const b = 'configurator-backup/2026-10-05_10-00-00/';
  assert.strictEqual(all[`${b}Mk2_3phase_RFdatalog_temp/config.h`], 'old');
  assert.ok(!(`${b}Mk2_3phase_RFdatalog_temp/calibration.h` in all));
  assert.deepStrictEqual(JSON.parse(all[`${b}manifest.json`]), {
    date: '2026-10-05_10-00-00',
    created: ['Mk2_3phase_RFdatalog_temp/config_system.h', 'mk2pvrouter.yaml', 'RemoteLoadReceiver-unit1'],
    removed: [],
  });
});

test('from 2 remote units to 1: the copy of unit 2 goes into the backup', async () => {
  const root = firmware();
  const two = [...FILES, { path: 'RemoteLoadReceiver-unit2/config.h', text: 'unit 2' }, { path: 'RemoteLoadReceiver-unit2/config_rf.h', text: 'unit 2 rf' }];
  await saveAll(root, two, '2026-10-05_10-00-00');
  root.items.get('RemoteLoadReceiver-unit2').items.set('.pio', new DirHandle('.pio', { build: 'x' }));

  const { p } = await saveAll(root, FILES, '2026-10-05_11-00-00');
  assert.deepStrictEqual(p.stale, ['RemoteLoadReceiver-unit2']);
  const all = flat(root);
  assert.ok(!root.items.has('RemoteLoadReceiver-unit2'));
  const b = 'configurator-backup/2026-10-05_11-00-00/';
  assert.strictEqual(all[`${b}RemoteLoadReceiver-unit2/config.h`], 'unit 2');
  assert.strictEqual(all[`${b}RemoteLoadReceiver-unit2/RemoteLoadReceiver-unit2.ino`], 'rx ino');
  assert.ok(!(`${b}RemoteLoadReceiver-unit2/.pio/build` in all));
  assert.deepStrictEqual(JSON.parse(all[`${b}manifest.json`]).removed, ['RemoteLoadReceiver-unit2']);
});

test('restoring the last backup gives back the folder as it was before that save', async () => {
  const root = firmware();
  const two = [...FILES, { path: 'RemoteLoadReceiver-unit2/config.h', text: 'unit 2' }];
  await saveAll(root, two, '2026-10-05_10-00-00');
  const before = flat(root);

  await saveAll(root, [{ path: 'Mk2_3phase_RFdatalog_temp/config.h', text: 'newer' }, { path: 'RemoteLoadReceiver-unit1/config.h', text: 'unit 1 bis' }, { path: 'other.yaml', text: 'y' }], '2026-10-05_11-00-00');
  assert.ok(!root.items.has('RemoteLoadReceiver-unit2'));

  const last = await F.lastBackup(root);
  assert.strictEqual(last.name, '2026-10-05_11-00-00');
  await F.restore(last);
  const after = flat(root);
  for (const k of Object.keys(after)) if (k.startsWith('configurator-backup/2026-10-05_11-00-00/')) delete after[k];
  assert.deepStrictEqual(after, before);
});

test('restoring the first save removes what it created', async () => {
  const root = firmware();
  const original = flat(root);
  await saveAll(root, FILES, '2026-10-05_10-00-00');
  await F.restore(await F.lastBackup(root));
  const after = flat(root);
  for (const k of Object.keys(after)) if (k.startsWith('configurator-backup/')) delete after[k];
  assert.deepStrictEqual(after, original);
});

test('the router folder itself: only the router files, the backup inside it', async () => {
  const root = firmware();
  const router = root.items.get('Mk2_3phase_RFdatalog_temp');
  const { p, backup } = await saveAll(router, FILES, '2026-10-05_10-00-00');
  assert.deepStrictEqual(
    p.skip.map((f) => f.path),
    ['RemoteLoadReceiver-unit1/config.h', 'RemoteLoadReceiver-unit1/config_rf.h', 'mk2pvrouter.yaml']
  );
  const all = flat(router);
  assert.strictEqual(all['config.h'], 'new');
  assert.strictEqual(all[`${backup}/config.h`], 'old');
  assert.ok(!root.items.has('RemoteLoadReceiver-unit1'));
  await F.restore(await F.lastBackup(router));
  assert.strictEqual(flat(router)['config.h'], 'old');
  assert.ok(!('config_system.h' in flat(router)));
});

test('another folder is refused, and nothing is written', async () => {
  const root = new DirHandle('Documents', { 'notes.txt': 'x' });
  assert.deepStrictEqual(await F.prepare(root, FILES), { error: 'folder.wrong' });
  assert.deepStrictEqual(await F.lastBackup(root), { error: 'folder.wrong' });
  assert.deepStrictEqual([...root.items.keys()], ['notes.txt']);
});

test('no backup yet', async () => {
  assert.deepStrictEqual(await F.lastBackup(firmware()), { error: 'folder.noBackup' });
});

test('remote units without the receiver sketch next to the router are refused', async () => {
  const root = firmware();
  root.items.delete('RemoteLoadReceiver');
  assert.deepStrictEqual(await F.prepare(root, FILES), { error: 'folder.noReceiver' });
  const p = await F.prepare(root, FILES.slice(0, 2));
  assert.ok(!p.error);
});
