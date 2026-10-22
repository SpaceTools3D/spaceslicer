// Run with Node.js; no dependencies. Exercise desktop commands and view transitions.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.resolve(__dirname, '../..');
const commands = [];
const states = new Map();
class Element {
    constructor(tag) { this.tag = tag; this.children = []; this.attributes = {}; this.events = {}; }
    appendChild(child) { this.children.push(child); }
    replaceChildren() { this.children = []; }
    setAttribute(key, value) { this.attributes[key] = value; }
    addEventListener(key, fn) { this.events[key] = fn; }
}
const list = new Element('div');
const document = {
    createElement: tag => new Element(tag),
    getElementById: id => id === 'FileList' ? list : null
};
const $ = selector => {
    if (!states.has(selector)) states.set(selector, {classes: new Set(), visible: true});
    const state = states.get(selector);
    const chain = {
        length: selector === '.FileItem' ? list.children.length : 0,
        toggleClass(c, on) { on ? state.classes.add(c) : state.classes.delete(c); return chain; },
        addClass(c) { state.classes.add(c); return chain; },
        removeClass(c) { state.classes.delete(c); return chain; },
        toggle(on) { state.visible = on; return chain; },
        show() { state.visible = true; return chain; },
        hide() { state.visible = false; return chain; },
        mousedown() { return chain; }, bind() { return chain; }, mousemove() { return chain; }, click() { return chain; }
    };
    return chain;
};
const context = vm.createContext({window:{}, document, $, Date, encodeURI, decodeURI,
    T: (_key, fallback) => fallback,
    SendWXMessage: value => commands.push(JSON.parse(value))
});
vm.runInContext(fs.readFileSync(path.join(root,'resources/web/homepage/js/home.js'),'utf8'), context);
context.OpenSpaceTools('login');
assert.equal(commands.pop().command, 'homepage_spacetools_login');
context.OpenSpaceTools('library');
assert.equal(commands.pop().command, 'homepage_spacetools_library');
context.SetSpaceToolsMode(true); // Native acknowledgement, after the pane opens.
assert.equal(context.m_SpaceToolsMode, true);
assert.equal(context.m_CloudMode, false);
assert.equal(states.get('body').classes.has('cloud-mode'), true);
context.SetCloudMode(true);
assert.equal(context.m_SpaceToolsMode, false);
assert.equal(context.m_CloudMode, true);
context.SetSpaceToolsMode(true);
assert.equal(context.m_CloudMode, false);
context.GotoMenu('recent');
assert.equal(commands.pop().command, 'homepage_spacetools_close');
assert.equal(states.get('body').classes.has('cloud-mode'), false);
const before = commands.length;
context.OpenSpaceTools('https://example.com');
assert.equal(commands.length, before, 'Only named, app-defined routes may be requested');
context.ShowRecentFileList([]);
assert.equal(states.get('#SpaceEmpty').visible, true);
const filename = '<img src=x onerror="alert(1)">';
const filePath = '/models/Mars & Moon\'s print.3mf';
context.ShowRecentFileList([{path:filePath,project_name:filename,time:'2026-10-20',published:'1'}]);
assert.equal(states.get('#SpaceEmpty').visible, false);
assert.equal(list.children[0].attributes.fpath, filePath);
assert.equal(list.children[0].children[1].children[1].textContent, filename,
    'A project name is text, never interpreted as markup');
list.children[0].events.keydown({key:'Enter',preventDefault(){}});
assert.equal(commands.at(-1).command, 'homepage_open_recentfile');
assert.equal(commands.at(-1).data.path, filePath);
console.log('Workspace commands, view transitions, empty state, safe names and keyboard opening passed.');
