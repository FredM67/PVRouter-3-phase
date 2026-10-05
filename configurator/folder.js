/*
 * PVRouter configurator - saving the files straight into the firmware folder, with the File System
 * Access API (Chrome, Edge). Works on directory handles only, so the tests can use a mock.
 *
 * prepare(root, files) -> { error } or { place, write, skip }
 * save(prepared)       -> writes prepared.write
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.PVRFolder = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const ROUTER = 'Mk2_3phase_RFdatalog_temp';
  const RECEIVER = 'RemoteLoadReceiver';

  const supported = () => typeof self !== 'undefined' && typeof self.showDirectoryPicker === 'function';

  async function child(dir, name, kind) {
    try {
      return kind === 'directory' ? await dir.getDirectoryHandle(name) : await dir.getFileHandle(name);
    } catch {
      return null;
    }
  }

  // The picked folder is the router folder itself, or the folder above it (the extracted firmware).
  // Only the latter gives access to the receiver and to the place of the YAML.
  async function prepare(root, files) {
    let place = null;
    if (await child(root, `${ROUTER}.ino`, 'file')) place = { name: root.name, router: root, top: null };
    else {
      const router = await child(root, ROUTER, 'directory');
      if (router && (await child(router, `${ROUTER}.ino`, 'file'))) place = { name: root.name, router, top: root };
    }
    if (!place) return { error: 'folder.wrong' };

    const write = files.filter((f) => place.top || f.path.startsWith(`${ROUTER}/`));
    const skip = files.filter((f) => !write.includes(f));
    if (place.top && write.some((f) => f.path.startsWith(`${RECEIVER}-unit`))) {
      place.receiver = await child(place.top, RECEIVER, 'directory');
      if (!place.receiver || !(await child(place.receiver, `${RECEIVER}.ino`, 'file'))) return { error: 'folder.noReceiver' };
    }
    return { place, write, skip };
  }

  async function writeFile(dir, name, data) {
    const w = await (await dir.getFileHandle(name, { create: true })).createWritable();
    await w.write(data);
    await w.close();
  }

  // The receiver's files, not its folders (.pio, .vscode); its configs are written right after.
  // The Arduino IDE wants the .ino named like its folder: RemoteLoadReceiver-unit1.ino.
  async function copySketch(from, to, folder) {
    for await (const [name, handle] of from.entries()) {
      if (handle.kind !== 'file' || name === 'config.h' || name === 'config_rf.h') continue;
      await writeFile(to, name === `${RECEIVER}.ino` ? `${folder}.ino` : name, await handle.getFile());
    }
  }

  async function save({ place, write }) {
    const copied = new Set();
    for (const f of write) {
      const parts = f.path.split('/');
      const name = parts.pop();
      let dir;
      if (!parts.length) dir = place.top;
      else if (parts[0] === ROUTER) dir = place.router;
      else {
        dir = await place.top.getDirectoryHandle(parts[0], { create: true });
        if (!copied.has(parts[0])) {
          await copySketch(place.receiver, dir, parts[0]);
          copied.add(parts[0]);
        }
      }
      await writeFile(dir, name, f.text);
    }
  }

  return { supported, prepare, save };
});
