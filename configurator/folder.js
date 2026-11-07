/*
 * PVRouter configurator - saving the files straight into the firmware folder, with the File System
 * Access API (Chrome, Edge). Works on directory handles only, so the tests can use a mock.
 *
 * Every save first copies what it replaces into configurator-backup/<stamp>/, with a manifest of
 * what it creates, so that the last save can be undone. Receiver copies of remote units that are
 * no longer used are moved into the backup too.
 *
 * prepare(root, files, { cleanup }) -> { error } or { place, write, skip, stale, units }
 * save(prepared, stamp) -> the backup's path
 * lastBackup(root)      -> { error } or { place, dir, name, manifest }
 * restore(backup)
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.PVRFolder = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const ROUTER = 'Mk2_3phase_RFdatalog_temp';
  const RECEIVER = 'RemoteLoadReceiver';
  const UNIT = /^RemoteLoadReceiver-unit(\d+)$/;
  const BACKUP = 'configurator-backup';
  const MANIFEST = 'manifest.json';

  const supported = () => typeof self !== 'undefined' && typeof self.showDirectoryPicker === 'function';

  async function child(dir, name, kind) {
    try {
      return kind === 'directory' ? await dir.getDirectoryHandle(name) : await dir.getFileHandle(name);
    } catch {
      return null;
    }
  }

  // the handle at 'a/b/c' under dir, or null
  async function at(dir, path, kind) {
    const parts = path.split('/');
    const last = parts.pop();
    for (const p of parts) if (!(dir = await child(dir, p, 'directory'))) return null;
    return child(dir, last, kind);
  }

  async function dirOf(dir, path, create) {
    for (const p of path.split('/').slice(0, -1)) dir = await dir.getDirectoryHandle(p, { create });
    return dir;
  }

  const last = (path) => path.split('/').pop();

  async function writeFile(dir, name, data) {
    const w = await (await dir.getFileHandle(name, { create: true })).createWritable();
    await w.write(data);
    await w.close();
  }

  // a folder's files and subfolders, build output (.pio) aside
  async function copyTree(from, to) {
    for await (const [name, handle] of from.entries()) {
      if (handle.kind === 'file') await writeFile(to, name, await handle.getFile());
      else if (name !== '.pio') await copyTree(handle, await to.getDirectoryHandle(name, { create: true }));
    }
  }

  // The picked folder is the router folder itself, or the folder above it (the extracted firmware).
  // Only the latter gives access to the receiver and to the place of the YAML. Paths are then
  // relative to the picked folder: 'config.h' or 'Mk2_3phase_RFdatalog_temp/config.h'.
  async function locate(root) {
    if (await child(root, `${ROUTER}.ino`, 'file')) return { name: root.name, base: root, router: root, top: null };
    const router = await child(root, ROUTER, 'directory');
    if (router && (await child(router, `${ROUTER}.ino`, 'file'))) return { name: root.name, base: root, router, top: root };
    return null;
  }

  // cleanup: false when the files are not the whole installation (calibration mode)
  async function prepare(root, files, { cleanup = true } = {}) {
    const place = await locate(root);
    if (!place) return { error: 'folder.wrong' };

    const mine = (f) => place.top || f.path.startsWith(`${ROUTER}/`);
    const write = files.filter(mine).map((f) => ({ path: place.top ? f.path : f.path.slice(ROUTER.length + 1), text: f.text }));
    const skip = files.filter((f) => !mine(f));
    const units = [...new Set(write.map((f) => f.path.split('/')[0]).filter((d) => UNIT.test(d)))];

    let stale = [];
    if (place.top) {
      if (units.length) {
        place.receiver = await child(place.top, RECEIVER, 'directory');
        if (!place.receiver || !(await child(place.receiver, `${RECEIVER}.ino`, 'file'))) return { error: 'folder.noReceiver' };
      }
      if (cleanup)
        for await (const [name, handle] of place.top.entries()) if (handle.kind === 'directory' && UNIT.test(name) && !units.includes(name)) stale.push(name);
      stale = stale.sort();
    }
    return { place, write, skip, stale, units };
  }

  // The receiver's files, not its folders (.pio, .vscode); its configs are written right after.
  // The Arduino IDE wants the .ino named like its folder: RemoteLoadReceiver-unit1.ino.
  async function copySketch(from, to, folder) {
    for await (const [name, handle] of from.entries()) {
      if (handle.kind !== 'file' || name === 'config.h' || name === 'config_rf.h') continue;
      await writeFile(to, name === `${RECEIVER}.ino` ? `${folder}.ino` : name, await handle.getFile());
    }
  }

  async function save({ place, write, stale, units }, stamp) {
    const { base } = place;
    const backup = await (await base.getDirectoryHandle(BACKUP, { create: true })).getDirectoryHandle(stamp, { create: true });
    const manifest = { date: stamp, created: [], removed: stale };

    // 1. the backup: replaced files, the receiver copies written into, the stale ones
    for (const f of write.filter((f) => !UNIT.test(f.path.split('/')[0]))) {
      const old = await at(base, f.path, 'file');
      if (old) await writeFile(await dirOf(backup, f.path, true), last(f.path), await old.getFile());
      else manifest.created.push(f.path);
    }
    for (const name of [...units, ...stale]) {
      const old = await child(base, name, 'directory');
      if (old) await copyTree(old, await backup.getDirectoryHandle(name, { create: true }));
      else manifest.created.push(name);
    }
    await writeFile(backup, MANIFEST, JSON.stringify(manifest, null, 2) + '\n');

    // 2. the receiver copies of units no longer used
    for (const name of stale) await base.removeEntry(name, { recursive: true });

    // 3. the files
    for (const name of units) {
      const dir = await base.getDirectoryHandle(name, { create: true });
      await copySketch(place.receiver, dir, name);
    }
    for (const f of write) await writeFile(await dirOf(base, f.path, true), last(f.path), f.text);
    return `${BACKUP}/${stamp}`;
  }

  // The most recent backup of the picked folder (stamps sort by date)
  async function lastBackup(root) {
    const place = await locate(root);
    if (!place) return { error: 'folder.wrong' };
    const backups = await child(place.base, BACKUP, 'directory');
    const names = [];
    if (backups) for await (const [name, handle] of backups.entries()) if (handle.kind === 'directory' && (await child(handle, MANIFEST, 'file'))) names.push(name);
    if (!names.length) return { error: 'folder.noBackup' };
    const name = names.sort().pop();
    const dir = await backups.getDirectoryHandle(name);
    const manifest = JSON.parse(await (await (await dir.getFileHandle(MANIFEST)).getFile()).text());
    return { place, dir, name, manifest };
  }

  // Removes what that save created, then puts back what it replaced or removed. A receiver copy
  // comes back whole, as it was; the router folder only gets its files back.
  async function restore({ place, dir, manifest }) {
    const { base } = place;
    const remove = async (path) => {
      const holder = path.includes('/') ? await at(base, path.slice(0, path.lastIndexOf('/')), 'directory') : base;
      if (holder && ((await child(holder, last(path), 'file')) || (await child(holder, last(path), 'directory')))) await holder.removeEntry(last(path), { recursive: true });
    };
    for (const path of manifest.created) await remove(path);
    for await (const [name, handle] of dir.entries()) {
      if (name === MANIFEST) continue;
      if (handle.kind === 'file') await writeFile(base, name, await handle.getFile());
      else {
        if (UNIT.test(name)) await remove(name);
        await copyTree(handle, await base.getDirectoryHandle(name, { create: true }));
      }
    }
  }

  return { supported, prepare, save, lastBackup, restore };
});
