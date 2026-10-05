// Saving into the firmware folder, on an in-memory folder that behaves like the browser's handles.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const F = require('../folder.js');

class FileHandle {
  constructor(name, content) {
    this.kind = 'file';
    this.name = name;
    this.content = content;
  }
  async getFile() {
    return this.content;
  }
  async createWritable() {
    let data;
    return { write: async (d) => (data = d), close: async () => (this.content = data) };
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
  async *entries() {
    yield* this.items.entries();
  }
}

const content = (dir, path) => path.split('/').reduce((d, p) => d && d.items.get(p), dir)?.content;

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

test('the extracted firmware folder: everything is saved, calibration.h is kept', async () => {
  const root = firmware();
  const p = await F.prepare(root, FILES);
  assert.deepStrictEqual(p.skip, []);
  await F.save(p);
  assert.strictEqual(content(root, 'Mk2_3phase_RFdatalog_temp/config.h'), 'new');
  assert.strictEqual(content(root, 'Mk2_3phase_RFdatalog_temp/config_system.h'), 'system');
  assert.strictEqual(content(root, 'Mk2_3phase_RFdatalog_temp/calibration.h'), 'mine');
  assert.strictEqual(content(root, 'RemoteLoadReceiver-unit1/RemoteLoadReceiver-unit1.ino'), 'rx ino');
  assert.ok(!root.items.get('RemoteLoadReceiver-unit1').items.has('RemoteLoadReceiver.ino'));
  assert.strictEqual(content(root, 'RemoteLoadReceiver-unit1/receiver.cpp'), 'rx');
  assert.strictEqual(content(root, 'RemoteLoadReceiver-unit1/config.h'), 'unit 1');
  assert.strictEqual(content(root, 'RemoteLoadReceiver-unit1/config_rf.h'), 'unit 1 rf');
  assert.ok(!root.items.get('RemoteLoadReceiver-unit1').items.has('.pio'));
  assert.strictEqual(content(root, 'RemoteLoadReceiver/config.h'), 'rx old');
  assert.strictEqual(content(root, 'mk2pvrouter.yaml'), 'yaml');
});

test('the router folder itself: only the router files, the others are listed', async () => {
  const root = firmware();
  const router = root.items.get('Mk2_3phase_RFdatalog_temp');
  const p = await F.prepare(router, FILES);
  assert.deepStrictEqual(
    p.skip.map((f) => f.path),
    ['RemoteLoadReceiver-unit1/config.h', 'RemoteLoadReceiver-unit1/config_rf.h', 'mk2pvrouter.yaml']
  );
  await F.save(p);
  assert.strictEqual(content(router, 'config.h'), 'new');
  assert.ok(!router.items.has('RemoteLoadReceiver-unit1'));
});

test('another folder is refused, and nothing is written', async () => {
  const root = new DirHandle('Documents', { 'notes.txt': 'x' });
  assert.deepStrictEqual(await F.prepare(root, FILES), { error: 'folder.wrong' });
  assert.deepStrictEqual([...root.items.keys()], ['notes.txt']);
});

test('remote units without the receiver sketch next to the router are refused', async () => {
  const root = firmware();
  root.items.delete('RemoteLoadReceiver');
  assert.deepStrictEqual(await F.prepare(root, FILES), { error: 'folder.noReceiver' });
  const p = await F.prepare(root, FILES.slice(0, 2));
  assert.ok(!p.error);
});
