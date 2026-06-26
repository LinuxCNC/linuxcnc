import { reactive } from 'vue';
import {
  HalcmdClient,
  type PinInfo,
  type ParamInfo,
  type SignalInfo,
  type ComponentInfo,
  type FunctionInfo,
  type ThreadInfo,
  type HalStatus,
  type CmdResult,
} from '../generated/halcmd_client';
import { HalcmdWatchClient } from '../generated/halcmd_watch_client';

// Tree node representing HAL hierarchy
export interface TreeNode {
  name: string;       // short name (last segment)
  fullPath: string;   // full dotted path
  children: TreeNode[];
  isLeaf: boolean;
  kind?: 'pin' | 'param' | 'signal' | 'component' | 'function' | 'thread';
  expanded?: boolean;
}

export type TabId = 'show' | 'watch' | 'cmd';

export interface WatchValueItem {
  name: string;
  type: string;
  dir: string;
  kind: string;
  value: string;
  owner: string;
  linked: boolean;
}

export interface CmdHistoryEntry {
  cmd: string;
  output?: string;
  error?: string;
}

export type TreeCategory = 'pins' | 'params' | 'signals' | 'components' | 'functions' | 'threads' | 'api';

export interface ApiFuncInfo {
  name: string;
  method?: string;
  path?: string;
}

export interface ApiWatchInfo {
  name: string;
  default_rate_ms: number;
}

export interface ApiInfo {
  api_name: string;
  instance: string;
  version: number;
  rest: boolean;
  functions?: ApiFuncInfo[];
  watches?: ApiWatchInfo[];
  commands?: string[];
  consumers?: string[];
}

interface HalshowState {
  // Connection
  connected: boolean;
  error: string;

  // HAL data
  pins: PinInfo[];
  params: ParamInfo[];
  signals: SignalInfo[];
  components: ComponentInfo[];
  functions: FunctionInfo[];
  threads: ThreadInfo[];
  status: HalStatus | null;

  // Tree
  treeCategory: TreeCategory;
  treeFilter: string;
  treeNodes: TreeNode[];
  selectedNode: TreeNode | null;

  // Detail (Show tab)
  selectedItem: PinInfo | ParamInfo | SignalInfo | ComponentInfo | FunctionInfo | ThreadInfo | null;
  selectedItemKind: TreeCategory | null;

  // Watch tab
  watchList: string[];     // names of items being watched
  watchValues: WatchValueItem[];  // live values from WebSocket
  watchRate: number;       // ms

  // Halcmd tab
  cmdHistory: CmdHistoryEntry[];

  // Node overview
  nodeOverviewPins: PinInfo[];

  // API registry
  apiRegistry: ApiInfo[];
  selectedApi: ApiInfo | null;

  // Active tab
  activeTab: TabId;
}

const state = reactive<HalshowState>({
  connected: false,
  error: '',

  pins: [],
  params: [],
  signals: [],
  components: [],
  functions: [],
  threads: [],
  status: null,

  treeCategory: 'pins',
  treeFilter: '',
  treeNodes: [],
  selectedNode: null,

  selectedItem: null,
  selectedItemKind: null,

  watchList: [],
  watchValues: [],
  watchRate: 100,

  cmdHistory: [],
  nodeOverviewPins: [],

  apiRegistry: [],
  selectedApi: null,

  activeTab: 'show',
});

let client: HalcmdClient;
let watchClient: HalcmdWatchClient;

const WATCH_STORAGE_KEY = 'halshow-watch-list';

function saveWatchList(names: string[]) {
  try {
    localStorage.setItem(WATCH_STORAGE_KEY, JSON.stringify(names));
  } catch { /* quota or private mode — ignore */ }
}

function loadWatchList(): string[] {
  try {
    const raw = localStorage.getItem(WATCH_STORAGE_KEY);
    if (raw) {
      const arr = JSON.parse(raw);
      if (Array.isArray(arr)) return arr.filter((s): s is string => typeof s === 'string');
    }
  } catch { /* corrupt data — ignore */ }
  return [];
}

function buildTree(items: { name: string }[], kind: TreeCategory): TreeNode[] {
  const root: TreeNode[] = [];
  const map = new Map<string, TreeNode>();

  const leafKind = kind === 'pins' ? 'pin' : kind === 'params' ? 'param'
    : kind === 'signals' ? 'signal' : kind === 'components' ? 'component'
    : kind === 'functions' ? 'function' : 'thread';

  for (const item of items) {
    const parts = item.name.split('.');
    let parent = root;
    let path = '';

    for (let i = 0; i < parts.length; i++) {
      const segment = parts[i];
      path = path ? path + '.' + segment : segment;
      const isLeaf = i === parts.length - 1;

      let node = map.get(path);
      if (!node) {
        node = {
          name: segment,
          fullPath: path,
          children: [],
          isLeaf,
          kind: isLeaf ? leafKind : undefined,
          expanded: false,
        };
        map.set(path, node);
        parent.push(node);
      }
      parent = node.children;
    }
  }

  return root;
}

function filterTree(nodes: TreeNode[], filter: string): TreeNode[] {
  if (!filter) return nodes;
  const lower = filter.toLowerCase();
  const result: TreeNode[] = [];
  for (const node of nodes) {
    if (node.fullPath.toLowerCase().includes(lower)) {
      result.push(node);
    } else if (!node.isLeaf) {
      const filteredChildren = filterTree(node.children, filter);
      if (filteredChildren.length > 0) {
        result.push({ ...node, children: filteredChildren, expanded: true });
      }
    }
  }
  return result;
}

export const halshowStore = {
  state,

  async connect() {
    const origin = window.location.origin;
    client = new HalcmdClient(origin);

    try {
      await this.refresh();
      state.connected = true;
      state.error = '';
    } catch (e) {
      state.error = e instanceof Error ? e.message : String(e);
    }

    // Connect WebSocket for watch
    const wsProto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${wsProto}//${window.location.host}/api/v1/watch`;
    watchClient = new HalcmdWatchClient(wsUrl);
    try {
      await watchClient.connect();
      watchClient.onClose = () => {
        state.connected = false;
      };
      // Restore saved watch list now that WebSocket is ready
      this.restoreWatchList();
    } catch {
      // Watch is optional — REST still works
    }
  },

  async refresh() {
    const [pins, params, signals, components, functions, threads, status] = await Promise.all([
      client.listPins(),
      client.listParams(),
      client.listSignals(),
      client.listComponents(),
      client.listFunctions(),
      client.listThreads(),
      client.getStatus(),
    ]);
    state.pins = pins;
    state.params = params;
    state.signals = signals;
    state.components = components;
    state.functions = functions;
    state.threads = threads;
    state.status = status;
    this.rebuildTree();
  },

  rebuildTree() {
    const items = this.getCategoryItems(state.treeCategory);
    const raw = buildTree(items, state.treeCategory);
    state.treeNodes = filterTree(raw, state.treeFilter);
  },

  getCategoryItems(cat: TreeCategory): { name: string }[] {
    switch (cat) {
      case 'pins': return state.pins;
      case 'params': return state.params;
      case 'signals': return state.signals;
      case 'components': return state.components;
      case 'functions': return state.functions;
      case 'threads': return state.threads;
      case 'api': return [];
    }
  },

  setCategory(cat: TreeCategory) {
    state.treeCategory = cat;
    state.selectedNode = null;
    state.selectedItem = null;
    state.selectedItemKind = null;
    if (cat === 'api') {
      this.refreshApiRegistry();
    } else {
      this.rebuildTree();
    }
  },

  setFilter(filter: string) {
    state.treeFilter = filter;
    this.rebuildTree();
  },

  async selectNode(node: TreeNode) {
    state.selectedNode = node;
    if (!node.isLeaf) {
      // Non-leaf: show overview of all child pins
      state.nodeOverviewPins = [];
      state.selectedItem = null;
      state.selectedItemKind = null;
      try {
        const allLeaves = this.collectLeaves(node);
        if (state.treeCategory === 'pins') {
          const pins = await Promise.all(allLeaves.map(n => client.getPin(n.fullPath)));
          state.nodeOverviewPins = pins;
        } else if (state.treeCategory === 'params') {
          // Show params as PinInfo-compatible for the overview table
          const params = await Promise.all(allLeaves.map(n => client.getParam(n.fullPath)));
          state.nodeOverviewPins = params.map(p => ({
            name: p.name, type: p.type, dir: p.dir, value: p.value,
            owner: p.owner, linked: false,
          }));
        }
      } catch (e) {
        state.error = e instanceof Error ? e.message : String(e);
      }
      return;
    }

    state.nodeOverviewPins = [];
    state.selectedItemKind = state.treeCategory;
    try {
      switch (state.treeCategory) {
        case 'pins':
          state.selectedItem = await client.getPin(node.fullPath);
          break;
        case 'params':
          state.selectedItem = await client.getParam(node.fullPath);
          break;
        case 'signals':
          state.selectedItem = await client.getSignal(node.fullPath);
          break;
        default:
          // For components/functions/threads, find from local data
          state.selectedItem = this.getCategoryItems(state.treeCategory)
            .find(i => i.name === node.fullPath) as typeof state.selectedItem;
      }
    } catch (e) {
      state.error = e instanceof Error ? e.message : String(e);
    }
  },

  toggleNode(node: TreeNode) {
    node.expanded = !node.expanded;
  },

  collectLeaves(node: TreeNode): TreeNode[] {
    const leaves: TreeNode[] = [];
    const visit = (n: TreeNode) => {
      if (n.isLeaf) {
        leaves.push(n);
      } else {
        for (const child of n.children) visit(child);
      }
    };
    visit(node);
    return leaves;
  },

  // --- Watch Tab ---

  addToWatch(name: string) {
    if (!state.watchList.includes(name)) {
      state.watchList.push(name);
      saveWatchList(state.watchList);
      this.updateWatch();
    }
  },

  isWatched(name: string): boolean {
    return state.watchList.includes(name);
  },

  removeFromWatch(name: string) {
    const idx = state.watchList.indexOf(name);
    if (idx >= 0) {
      state.watchList.splice(idx, 1);
      saveWatchList(state.watchList);
      this.updateWatch();
    }
  },

  clearWatch() {
    state.watchList = [];
    state.watchValues = [];
    saveWatchList(state.watchList);
    watchClient?.unsubscribeWatchItems();
  },

  /** Restore watch list from localStorage, dropping pins/params that no longer exist. */
  restoreWatchList() {
    const saved = loadWatchList();
    if (saved.length === 0) return;

    const knownNames = new Set<string>();
    for (const p of state.pins) knownNames.add(p.name);
    for (const p of state.params) knownNames.add(p.name);
    for (const s of state.signals) knownNames.add(s.name);

    const valid = saved.filter(n => knownNames.has(n));
    if (valid.length !== saved.length) {
      saveWatchList(valid); // prune stale entries
    }
    if (valid.length > 0) {
      state.watchList = valid;
      state.activeTab = 'watch';
      this.updateWatch();
    }
  },

  updateWatch() {
    if (state.watchList.length === 0) {
      watchClient?.unsubscribeWatchItems();
      state.watchValues = [];
      return;
    }

    // Seed maps from existing watchValues so old items stay visible during
    // re-subscription.  The new subscription's meta response will replace
    // these with fresh data once it arrives.
    const metaMap = new Map<string, { type: string; dir: string; kind: string; owner: string; linked: boolean }>();
    const valueMap = new Map<string, string>();
    for (const v of state.watchValues) {
      metaMap.set(v.name, { type: v.type, dir: v.dir, kind: v.kind, owner: v.owner, linked: v.linked });
      valueMap.set(v.name, v.value);
    }

    watchClient?.subscribeWatchItems((data: unknown) => {
      const msg = data as Record<string, unknown>;

      if (msg.meta && Array.isArray(msg.meta)) {
        // First message (or structure change): contains metadata + initial values
        const metaNames = (msg.meta as Array<{ name: string }>).map(m => m.name);
        console.log('[watch] meta received:', metaNames.length, 'items:', metaNames);
        metaMap.clear();
        for (const m of msg.meta as Array<{ name: string; type: string; dir: string; kind: string; owner: string; linked: boolean }>) {
          metaMap.set(m.name, { type: m.type, dir: m.dir ?? '', kind: m.kind ?? '', owner: m.owner, linked: m.linked });
        }
        const values = (msg.values ?? {}) as Record<string, string>;
        for (const [name, value] of Object.entries(values)) {
          valueMap.set(name, value);
        }
      } else {
        // Subsequent messages: only changed name→value pairs
        const keys = Object.keys(msg);
        if (keys.length > 0) {
          console.log('[watch] delta:', keys.length, 'changed');
        }
        for (const [name, value] of Object.entries(msg)) {
          valueMap.set(name, value as string);
        }
      }

      // Rebuild watchValues array from metadata + current values.
      // Items not yet in metaMap (newly added, waiting for server meta) are
      // excluded — the template shows them from watchList with '—' fallback.
      state.watchValues = state.watchList
        .filter(n => metaMap.has(n))
        .map(n => {
          const meta = metaMap.get(n)!;
          return {
            name: n,
            type: meta.type,
            dir: meta.dir,
            kind: meta.kind,
            value: valueMap.get(n) ?? '—',
            owner: meta.owner,
            linked: meta.linked,
          };
        });
    }, state.watchRate, state.watchList);
  },

  // --- Mutations ---

  async setValue(name: string, value: string, kind: 'pin' | 'param' | 'signal'): Promise<CmdResult> {
    let result: CmdResult;
    switch (kind) {
      case 'pin':
        result = await client.setPin(name, value);
        break;
      case 'param':
        result = await client.setParam(name, value);
        break;
      case 'signal':
        result = await client.setSignal(name, value);
        break;
    }
    return result;
  },

  async unlinkPin(name: string): Promise<CmdResult> {
    return await client.unlink(name);
  },

  // --- Node overview: add all to watch ---

  addAllNodePinsToWatch() {
    for (const pin of state.nodeOverviewPins) {
      if (!state.watchList.includes(pin.name)) {
        state.watchList.push(pin.name);
      }
    }
    saveWatchList(state.watchList);
    this.updateWatch();
  },

  // --- Watch: set value ---

  async setWatchValue(name: string, value: string): Promise<CmdResult> {
    // Determine if it's a pin, param, or signal
    if (state.pins.find(p => p.name === name)) {
      return await client.setPin(name, value);
    } else if (state.params.find(p => p.name === name)) {
      return await client.setParam(name, value);
    } else if (state.signals.find(s => s.name === name)) {
      return await client.setSignal(name, value);
    }
    return { success: false, error: 'Unknown item type' };
  },

  // --- Halcmd console ---

  async executeHalcmd(cmdLine: string) {
    const entry: CmdHistoryEntry = { cmd: cmdLine };
    state.cmdHistory.push(entry);

    try {
      const result = await this.parseAndExecute(cmdLine);
      if (result.output) entry.output = result.output;
      if (!result.success) entry.error = result.error ?? 'Failed';
    } catch (e) {
      entry.error = e instanceof Error ? e.message : String(e);
    }
    // Force reactivity by replacing the array
    state.cmdHistory = [...state.cmdHistory];
  },

  clearCmdHistory() {
    state.cmdHistory = [];
  },

  async parseAndExecute(cmdLine: string): Promise<CmdResult> {
    const tokens = cmdLine.trim().split(/\s+/);
    if (tokens.length === 0) return { success: true };
    const cmd = tokens[0];
    const args = tokens.slice(1);

    switch (cmd) {
      case 'show': {
        const what = args[0] ?? 'pin';
        const pattern = args[1];
        let output = '';
        if (what === 'pin' || what === 'pins') {
          const pins = await client.listPins(pattern);
          output = pins.map(p =>
            `${p.name.padEnd(40)} ${p.type.padEnd(6)} ${p.dir.padEnd(4)} ${p.value.padEnd(15)} ${p.linked ? '=> ' + p.signal : ''}`
          ).join('\n');
        } else if (what === 'param' || what === 'params') {
          const params = await client.listParams(pattern);
          output = params.map(p =>
            `${p.name.padEnd(40)} ${p.type.padEnd(6)} ${p.dir.padEnd(4)} ${p.value}`
          ).join('\n');
        } else if (what === 'sig' || what === 'signal' || what === 'signals') {
          const sigs = await client.listSignals(pattern);
          output = sigs.map(s =>
            `${s.name.padEnd(40)} ${s.type.padEnd(6)} ${s.value}`
          ).join('\n');
        } else if (what === 'comp' || what === 'components') {
          const comps = await client.listComponents(pattern);
          output = comps.map(c =>
            `${c.name.padEnd(30)} ${String(c.id).padEnd(6)} ${c.type.padEnd(12)} ${c.state}`
          ).join('\n');
        } else if (what === 'funct' || what === 'functions') {
          const funcs = await client.listFunctions(pattern);
          output = funcs.map(f =>
            `${f.name.padEnd(40)} ${f.owner.padEnd(20)} ${f.fp ? 'FP' : 'NO'}`
          ).join('\n');
        } else if (what === 'thread' || what === 'threads') {
          const threads = await client.listThreads(pattern);
          output = threads.map(t =>
            `${t.name.padEnd(30)} ${String(t.period).padEnd(12)} ${t.fp ? 'FP' : 'NO'}${t.functions.length > 0 ? '\n  ' + t.functions.join('\n  ') : ''}`
          ).join('\n');
        } else {
          return { success: false, error: `Unknown show type: ${what}` };
        }
        return { success: true, output: output || '(no results)' };
      }

      case 'getp':
      case 'gets': {
        const name = args[0];
        if (!name) return { success: false, error: `Usage: ${cmd} <name>` };
        if (cmd === 'getp') {
          const pin = await client.getPin(name);
          return { success: true, output: pin.value };
        } else {
          const sig = await client.getSignal(name);
          return { success: true, output: sig.value };
        }
      }

      case 'setp': {
        if (args.length < 2) return { success: false, error: 'Usage: setp <name> <value>' };
        // Try pin first, then param
        let result = await client.setPin(args[0], args[1]);
        if (!result.success) {
          result = await client.setParam(args[0], args[1]);
        }
        return result;
      }

      case 'sets': {
        if (args.length < 2) return { success: false, error: 'Usage: sets <signal> <value>' };
        return await client.setSignal(args[0], args[1]);
      }

      case 'net': {
        if (args.length < 2) return { success: false, error: 'Usage: net <signal> <pin> [pin...]' };
        return await client.net(args[0], args.slice(1));
      }

      case 'linkps': {
        if (args.length < 2) return { success: false, error: 'Usage: linkps <pin> <signal>' };
        return await client.link(args[0], args[1]);
      }

      case 'unlinkp': {
        if (args.length < 1) return { success: false, error: 'Usage: unlinkp <pin>' };
        return await client.unlink(args[0]);
      }

      case 'newsig': {
        if (args.length < 2) return { success: false, error: 'Usage: newsig <name> <type>' };
        return await client.newSignal(args[0], args[1]);
      }

      case 'delsig': {
        if (args.length < 1) return { success: false, error: 'Usage: delsig <name>' };
        return await client.deleteSignal(args[0]);
      }

      case 'loadrt': {
        if (args.length < 1) return { success: false, error: 'Usage: loadrt <module> [args...]' };
        return await client.loadrt(args[0], args.slice(1) as any);
      }

      case 'unloadrt': {
        if (args.length < 1) return { success: false, error: 'Usage: unloadrt <module>' };
        return await client.unloadrt(args[0]);
      }

      case 'start':
        return await client.start();

      case 'stop':
        return await client.stop();

      case 'status': {
        const st = await client.getStatus();
        return {
          success: true,
          output: `Components: ${st.components}  Pins: ${st.pins}  Signals: ${st.signals}  Params: ${st.params}  Threads: ${st.threads}  Functions: ${st.functions}\nRT lock: ${st.rt_lock}  Mem lock: ${st.mem_lock}  Running: ${st.threads_running}`,
        };
      }

      case 'help':
        return {
          success: true,
          output: [
            'show pin|param|sig|comp|funct|thread [pattern]',
            'getp <pin>           gets <signal>',
            'setp <name> <value>  sets <signal> <value>',
            'net <signal> <pin> [pin...]',
            'linkps <pin> <signal>  unlinkp <pin>',
            'newsig <name> <type>   delsig <name>',
            'loadrt <module> [args...]  unloadrt <module>',
            'start  stop  status',
          ].join('\n'),
        };

      default:
        return { success: false, error: `Unknown command: ${cmd}. Type "help" for available commands.` };
    }
  },

  setActiveTab(tab: TabId) {
    state.activeTab = tab;
  },

  async refreshApiRegistry() {
    try {
      const origin = window.location.origin;
      const resp = await fetch(origin + '/api/v1/_registry');
      if (resp.ok) {
        state.apiRegistry = await resp.json();
      }
    } catch {
      // Silently ignore — API tab shows empty
    }
  },

  selectApi(api: ApiInfo) {
    state.selectedApi = api;
  },
};
