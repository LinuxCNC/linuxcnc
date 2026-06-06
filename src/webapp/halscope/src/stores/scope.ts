import { reactive } from 'vue';
import {
  HalscopeClient,
  type ScopeStatus,
  type ThreadInfo,
  type CaptureConfig,
  type TriggerConfig,
  type ChannelConfig,
  ScopeState,
  TrigEdge,
  HalType,
  MAX_CHANNELS,
} from '../generated/halscope_client';
import { HalscopeWatchClient } from '../generated/halscope_watch_client';

// Per-channel UI settings (colors, vertical scale, offset)
export interface ChannelUI {
  color: string;
  vScale: number;   // units per division
  vOffset: number;  // vertical offset in divisions
  visible: boolean;
}

// Decoded sample data per channel
export interface ChannelSamples {
  channel: number;
  data: Float64Array;
}

const CHANNEL_COLORS = [
  '#ffff00', '#00ff00', '#ff4444', '#44aaff',
  '#ff44ff', '#44ffff', '#ff8800', '#88ff00',
  '#ff0088', '#0088ff', '#8800ff', '#00ff88',
  '#ffaa44', '#44ffaa', '#aa44ff', '#ff44aa',
];

interface ScopeStore {
  // Connection
  connected: boolean;
  error: string;

  // Status from server
  status: ScopeStatus;

  // Threads
  threads: ThreadInfo[];

  // Available pins
  pins: string[];
  pinFilter: string;

  // Capture config
  captureConfig: CaptureConfig;

  // Trigger config
  triggerConfig: TriggerConfig;

  // Channel UI settings
  channelUI: ChannelUI[];

  // Sample data
  samples: ChannelSamples[];
  timeBase: Float64Array; // time axis in seconds

  // UI state
  selectedThread: string;
  selectedChannel: number; // -1 = none selected

  // Horizontal display (ported from scope_horiz_t)
  zoomSetting: number;   // 1..9, 1 = fit record
  posSetting: number;    // 0.0..1.0, position within record

  // Cursor state (set by chart mouse events)
  cursorTime: number | null;     // hover time in seconds
  cursorValue: number | null;    // hover value in real units (selected channel)
  dragStartTime: number | null;  // drag anchor time
  dragStartValue: number | null; // drag anchor value
  dragDeltaTime: number | null;  // delta from drag start
  dragDeltaValue: number | null; // delta from drag start
  isDragging: boolean;
}

const state = reactive<ScopeStore>({
  connected: false,
  error: '',

  status: {
    state: ScopeState.IDLE,
    samples: 0,
    recLen: 16000,
    preTrig: 8000,
    sampleLen: 0,
    maxChannels: 1,
    samplePeriodMult: 1,
    threadPeriodNs: 0,
    threadName: '',
    trigChannel: -1,
    generation: 0,
    continuous: false,
    channels: [],
    channelOptions: [],
  },

  threads: [],
  pins: [],
  pinFilter: '',

  captureConfig: {
    threadName: '',
    maxChannels: 1,
    samplePeriodMult: 1,
    preTrig: 0,
  },

  triggerConfig: {
    channel: -1,
    level: 0,
    edge: TrigEdge.RISING,
    autoTrig: true,
  },

  channelUI: Array.from({ length: MAX_CHANNELS }, (_, i) => ({
    color: CHANNEL_COLORS[i % CHANNEL_COLORS.length],
    vScale: 1,
    vOffset: 0,
    visible: true,
  })),

  samples: [],
  timeBase: new Float64Array(0),

  selectedThread: '',
  selectedChannel: -1,

  zoomSetting: 1,
  posSetting: 0.5,

  cursorTime: null,
  cursorValue: null,
  dragStartTime: null,
  dragStartValue: null,
  dragDeltaTime: null,
  dragDeltaValue: null,
  isDragging: false,
});

let restClient: HalscopeClient | null = null;
let wsClient: HalscopeWatchClient | null = null;
let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
let reconnectDelay = 1000;
let configSynced = false;

function getBaseUrl(): string {
  return window.location.origin;
}

function getWsUrl(): string {
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  return `${proto}//${window.location.host}/api/v1/watch`;
}

// --- Actions ---

async function connect() {
  if (reconnectTimer) {
    clearTimeout(reconnectTimer);
    reconnectTimer = null;
  }

  try {
    if (!restClient) {
      restClient = new HalscopeClient(getBaseUrl());
    }

    // Load initial data via REST immediately (don't wait for WS)
    const [threads, status] = await Promise.all([
      restClient.listThreads(),
      restClient.getStatus(),
    ]);
    state.threads = threads;
    onStatusUpdate(status);

    if (threads.length > 0 && !state.selectedThread) {
      state.selectedThread = threads[0].name;
      state.captureConfig.threadName = threads[0].name;
    }
  } catch (e) {
    state.error = `REST connection failed: ${e}`;
    restClient = null;
    scheduleReconnect();
    return;
  }

  // Connect WS for live updates — failures here don't block the UI
  try {
    wsClient = new HalscopeWatchClient(getWsUrl());
    await wsClient.connect();
    wsClient.onClose = onWsClose;
    state.connected = true;
    state.error = '';
    reconnectDelay = 1000;

    // Subscribe to state updates
    wsClient.subscribeWatchState(onStatusUpdate, 100);

    // Subscribe to sample data
    wsClient.subscribeWatchSamples(onSamplesUpdate, 100);
  } catch (e) {
    state.error = `WebSocket failed, retrying…`;
    state.connected = false;
    scheduleReconnect();
  }
}

function scheduleReconnect() {
  if (reconnectTimer) return;
  reconnectTimer = setTimeout(() => {
    reconnectTimer = null;
    connect();
  }, reconnectDelay);
  reconnectDelay = Math.min(reconnectDelay * 2, 10000);
}

function disconnect() {
  if (reconnectTimer) {
    clearTimeout(reconnectTimer);
    reconnectTimer = null;
  }
  wsClient?.close();
  wsClient = null;
  restClient = null;
  state.connected = false;
}

function channelsChanged(a: typeof state.status.channels, b: typeof state.status.channels): boolean {
  if (a.length !== b.length) return true;
  for (let i = 0; i < a.length; i++) {
    if (a[i].channel !== b[i].channel || a[i].pinName !== b[i].pinName || a[i].enabled !== b[i].enabled)
      return true;
  }
  return false;
}

function onStatusUpdate(status: Partial<ScopeStatus>) {
  // WS delta updates only include changed fields — only update fields
  // that are actually present to avoid clobbering with undefined.
  if ('state' in status) state.status.state = status.state!;
  if ('samples' in status) state.status.samples = status.samples!;
  if ('recLen' in status) state.status.recLen = status.recLen!;
  if ('preTrig' in status) state.status.preTrig = status.preTrig!;
  if ('sampleLen' in status) state.status.sampleLen = status.sampleLen!;
  if ('maxChannels' in status) state.status.maxChannels = status.maxChannels!;
  if ('samplePeriodMult' in status) state.status.samplePeriodMult = status.samplePeriodMult!;
  if ('threadPeriodNs' in status) state.status.threadPeriodNs = status.threadPeriodNs!;
  if ('threadName' in status) state.status.threadName = status.threadName!;
  if ('trigChannel' in status) state.status.trigChannel = status.trigChannel!;
  if ('generation' in status) state.status.generation = status.generation!;
  if ('continuous' in status) state.status.continuous = status.continuous!;
  if ('channelOptions' in status) state.status.channelOptions = status.channelOptions!;

  // Only replace channels array if present and content actually changed
  if ('channels' in status) {
    const channels = status.channels ?? [];
    if (channelsChanged(state.status.channels, channels)) {
      state.status.channels = channels;
    }
  }

  // Sync capture/trigger config from server only on first status after
  // (re-)connect.  Once synced, the UI owns these values — the watch
  // stream must not clobber in-flight edits.
  if (!configSynced) {
    configSynced = true;
    if ('maxChannels' in status && status.maxChannels! > 0) {
      state.captureConfig.maxChannels = status.maxChannels!;
    }
    if ('samplePeriodMult' in status && status.samplePeriodMult! > 0) {
      state.captureConfig.samplePeriodMult = status.samplePeriodMult!;
    }
    if ('threadName' in status && status.threadName) {
      state.captureConfig.threadName = status.threadName;
      state.selectedThread = status.threadName;
    }
    if ('trigChannel' in status) {
      state.triggerConfig.channel = status.trigChannel!;
    }
    if ('trigLevel' in status) {
      state.triggerConfig.level = status.trigLevel!;
    }
    if ('trigEdge' in status) {
      state.triggerConfig.edge = status.trigEdge!;
    }
    if ('trigAutoTrig' in status) {
      state.triggerConfig.autoTrig = status.trigAutoTrig!;
    }
  }
}

function onWsClose() {
  state.connected = false;
  wsClient = null;
  configSynced = false;
  scheduleReconnect();
}

function onSamplesUpdate(buf: ArrayBuffer) {
  if (buf.byteLength < 16) return;

  // Binary layout: sample_header_t (16 bytes, 4× uint32 LE) + sample data (float64 LE)
  // Header: { sample_count, sample_len, start_offset, reserved }
  const view = new DataView(buf);
  const sampleCount = view.getUint32(0, true);
  const sampleLen = view.getUint32(4, true);
  const startOffset = view.getUint32(8, true);

  if (sampleLen === 0 || sampleCount === 0) return;

  const dataOffset = 16; // header size in bytes
  const channels = state.status.channels.filter(c => c.enabled);

  // Create a Float64Array view over the data portion (header is 16 bytes = aligned to 8)
  const allSamples = new Float64Array(buf, dataOffset);

  const decoded: ChannelSamples[] = [];
  for (const ch of channels) {
    // Fixed-column layout: channel N is at column index N
    // Sample layout is [s0c0, s0c1, ..., s0cN, s1c0, s1c1, ...]
    if (ch.channel >= sampleLen) continue;
    const data = new Float64Array(sampleCount);
    for (let si = 0; si < sampleCount; si++) {
      data[si] = allSamples[si * sampleLen + ch.channel];
    }
    decoded.push({ channel: ch.channel, data });
  }

  state.samples = decoded;

  // Build time base
  const threadPeriodNs = getSelectedThreadPeriod();
  const dt = (threadPeriodNs * state.captureConfig.samplePeriodMult) / 1e9;
  const tb = new Float64Array(sampleCount);
  const t0 = -(startOffset * dt);
  for (let i = 0; i < sampleCount; i++) {
    tb[i] = t0 + i * dt;
  }
  state.timeBase = tb;
}

function getSelectedThreadPeriod(): number {
  const t = state.threads.find(t => t.name === state.captureConfig.threadName);
  return t?.periodNs ?? 1000000;
}

async function configure() {
  if (!restClient) return;
  try {
    state.captureConfig.threadName = state.selectedThread;
    // preTrig is always recLen/2 (hardcoded server-side, like original halscope)
    state.captureConfig.preTrig = 0;
    await restClient.configure(state.captureConfig);
    // Re-fetch status so recLen/preTrig reflect the new maxChannels
    const status = await restClient.getStatus();
    onStatusUpdate(status);
    state.error = '';
  } catch (e) {
    state.error = `Configure failed: ${e}`;
  }
}

async function addChannel(pinName: string, channel: number) {
  if (!restClient) return;
  try {
    const ch: ChannelConfig = { channel, pinName: pinName };
    const rc = await restClient.setChannel(ch);
    if (rc !== 0) {
      state.error = `Set channel failed: error code ${rc}`;
      return;
    }
    state.error = '';
    // Refresh status to see the new channel
    const status = await restClient.getStatus();
    onStatusUpdate(status);
  } catch (e) {
    state.error = `Set channel failed: ${e}`;
  }
}

async function removeChannel(channel: number) {
  if (!restClient) return;
  try {
    await restClient.clearChannel(channel);
    state.error = '';
  } catch (e) {
    state.error = `Clear channel failed: ${e}`;
  }
}

async function setTrigger() {
  if (!restClient) return;
  try {
    await restClient.setTrigger(state.triggerConfig);
    state.error = '';
  } catch (e) {
    state.error = `Set trigger failed: ${e}`;
  }
}

async function arm() {
  if (!restClient) return;
  try {
    // Always send current config + trigger before arming
    await configure();
    await setTrigger();
    await restClient.arm();
    state.error = '';
  } catch (e) {
    state.error = `Arm failed: ${e}`;
  }
}

async function stop() {
  if (!restClient) return;
  try {
    await restClient.setContinuous(false);
    await restClient.reset();
    state.error = '';
  } catch (e) {
    state.error = `Reset failed: ${e}`;
  }
}

async function fullReset() {
  if (!restClient) return;
  try {
    // Stop any running capture
    await restClient.setContinuous(false);
    await restClient.reset();
    // Clear all channels on server
    for (let i = 0; i < state.status.channels.length; i++) {
      const ch = state.status.channels[i];
      if (ch.enabled) {
        await restClient.clearChannel(ch.channel);
      }
    }
    // Reset local UI state
    state.samples = [];
    state.timeBase = new Float64Array(0);
    state.selectedChannel = -1;
    state.triggerConfig.channel = -1;
    state.triggerConfig.level = 0;
    state.triggerConfig.edge = 0;
    state.triggerConfig.autoTrig = true;
    state.error = '';
    // Refresh status
    const status = await restClient.getStatus();
    onStatusUpdate(status);
  } catch (e) {
    state.error = `Full reset failed: ${e}`;
  }
}

async function run() {
  if (!restClient) return;
  try {
    await configure();
    await setTrigger();
    await restClient.setContinuous(true);
    await restClient.arm();
    state.error = '';
  } catch (e) {
    state.error = `Run failed: ${e}`;
  }
}

async function forceTrigger() {
  if (!restClient) return;
  try {
    await restClient.forceTrigger();
    state.error = '';
  } catch (e) {
    state.error = `Force trigger failed: ${e}`;
  }
}

// Send config immediately if not capturing (for live parameter changes)
async function applyConfig() {
  const s = state.status.state;
  if (s === ScopeState.IDLE || s === ScopeState.DONE) {
    await configure();
    await setTrigger();
  }
}

function isCapturing(): boolean {
  const s = state.status.state;
  return s === ScopeState.INIT || s === ScopeState.PRE_TRIG ||
    s === ScopeState.TRIG_WAIT || s === ScopeState.POST_TRIG;
}

async function searchPins(pattern?: string, kind?: string) {
  if (!restClient) return;
  try {
    state.pins = await restClient.listPins(pattern || undefined, kind || undefined) ?? [];
    state.error = '';
  } catch (e) {
    state.error = `List pins failed: ${e}`;
  }
}

/**
 * Calculate display scale (seconds per division) — exact port of
 * calc_horiz_scaling() from scope_horiz.c.
 *
 * Uses 1-2-5 sequence: at zoom=1, disp_scale shows the full record
 * across 10 divisions. Each zoom step divides by one 1-2-5 step.
 */
function calcDispScale(): number {
  const samplePeriod = getSamplePeriod();
  if (samplePeriod === 0) return 0;
  const totalRecTime = state.status.recLen * samplePeriod;
  if (totalRecTime < 0.000010) return 0.000001;

  const desiredUsecPerDiv = (totalRecTime / 10.0) * 1000000.0;

  // Find 1-2-5 value >= desired
  let decade = 1;
  let subDecade = 1;
  let actual = decade * subDecade;
  while (actual < desiredUsecPerDiv) {
    if (subDecade === 1) subDecade = 2;
    else if (subDecade === 2) subDecade = 5;
    else { subDecade = 1; decade *= 10; }
    actual = decade * subDecade;
  }

  // Zoom in: each step divides by one 1-2-5 step
  for (let n = 1; n < state.zoomSetting; n++) {
    if (subDecade === 1) { subDecade = 5; decade = Math.floor(decade / 10); }
    else if (subDecade === 2) subDecade = 1;
    else subDecade = 2;
  }
  if (decade === 0) { decade = 1; subDecade = 1; }

  return (decade * subDecade) / 1000000.0;
}

function getSamplePeriod(): number {
  // Use the actual thread period and mult from RT status
  const periodNs = state.status.threadPeriodNs || getSelectedThreadPeriod();
  const mult = state.status.samplePeriodMult || state.captureConfig.samplePeriodMult;
  return (periodNs * mult) / 1e9;
}

/**
 * Compute display window parameters — port of scope_disp.c display calc.
 * Returns start/end sample indices for chart slicing, plus time-domain
 * values needed by the buffer indicator.
 */
function calcDisplayWindow() {
  const samplePeriod = getSamplePeriod();
  const dispScale = calcDispScale();
  const recLen = state.status.recLen || 1;
  // Use actual preTrig from RT status (always recLen/2, set server-side)
  const preTrig = state.status.preTrig > 0 ? state.status.preTrig : Math.round(recLen / 2);

  // Record boundaries relative to trigger (t=0)
  const recStart = -preTrig * samplePeriod;
  const recEnd = (recLen - preTrig) * samplePeriod;

  // posSetting 0..1 maps across the record, trigger-relative
  const screenCenterTime = recStart + (recEnd - recStart) * state.posSetting;
  const screenStartTime = screenCenterTime - 5.0 * dispScale;
  const screenEndTime = screenCenterTime + 5.0 * dispScale;

  let startSample = Math.floor(screenStartTime / samplePeriod);
  if (startSample < 0) startSample = 0;
  let endSample = Math.ceil(screenEndTime / samplePeriod) + 1;
  if (endSample > recLen - 1) endSample = recLen - 1;

  return {
    samplePeriod,
    dispScale,
    recLen,
    preTrig,
    screenCenterTime,
    screenStartTime,
    screenEndTime,
    startSample,
    endSample,
  };
}

function setHorizZoom(setting: number) {
  state.zoomSetting = Math.max(1, Math.min(9, Math.round(setting)));
}

function setHorizPos(setting: number) {
  state.posSetting = Math.max(0, Math.min(1, setting));
}

const HAL_TYPE_NAMES: Record<number, string> = {
  [HalType.BIT]: 'BIT',
  [HalType.FLOAT]: 'FLOAT',
  [HalType.S32]: 'S32',
  [HalType.U32]: 'U32',
};

/** Export the current capture as semicolon-separated CSV and trigger a download. */
function saveCapture() {
  if (state.samples.length === 0 || state.timeBase.length === 0) {
    state.error = 'No capture data to save';
    return;
  }

  const channels = state.status.channels.filter(c => c.enabled);
  const sampleCount = state.timeBase.length;
  const periodNs = Math.round(getSamplePeriod() * 1e9);

  // Build comment header
  const lines: string[] = [];
  lines.push(`# halscope capture ${new Date().toISOString()}`);
  lines.push(`# sample_period_ns=${periodNs}`);
  const trigCh = state.triggerConfig.channel;
  if (trigCh >= 0) {
    const edgeName = state.triggerConfig.edge === TrigEdge.RISING ? 'rising' : 'falling';
    lines.push(`# trigger_channel=${trigCh} trigger_level=${state.triggerConfig.level} trigger_edge=${edgeName}`);
  }

  // Column header: time + channel names with type annotation
  const colHeaders = ['time_s'];
  for (const ch of channels) {
    const typeName = HAL_TYPE_NAMES[ch.dataType] ?? `TYPE${ch.dataType}`;
    colHeaders.push(`${ch.pinName}[${typeName}]`);
  }
  lines.push(colHeaders.join(';'));

  // Lookup sample arrays by channel index
  const sampleMap = new Map<number, Float64Array>();
  for (const s of state.samples) {
    sampleMap.set(s.channel, s.data);
  }

  // Data rows
  for (let i = 0; i < sampleCount; i++) {
    const row = [state.timeBase[i].toFixed(9)];
    for (const ch of channels) {
      const data = sampleMap.get(ch.channel);
      const v = data ? data[i] : 0;
      row.push(v.toFixed(14));
    }
    lines.push(row.join(';'));
  }

  const csv = lines.join('\n') + '\n';
  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `halscope_${new Date().toISOString().replace(/[:.]/g, '-')}.csv`;
  a.click();
  URL.revokeObjectURL(url);
}

/** Load a previously saved CSV capture file and display it. */
function loadCapture() {
  const input = document.createElement('input');
  input.type = 'file';
  input.accept = '.csv,.txt';
  input.onchange = () => {
    const file = input.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      try {
        parseAndLoadCapture(reader.result as string);
      } catch (e) {
        state.error = `Failed to load capture: ${e}`;
      }
    };
    reader.readAsText(file);
  };
  input.click();
}

function parseAndLoadCapture(text: string) {
  const lines = text.split('\n').filter(l => l.length > 0);

  // Parse comment headers
  let periodNs = 0;
  const comments: string[] = [];
  let dataStart = 0;
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].startsWith('#')) {
      comments.push(lines[i]);
      dataStart = i + 1;
      const m = lines[i].match(/sample_period_ns=(\d+)/);
      if (m) periodNs = Number(m[1]);
    } else {
      break;
    }
  }

  if (dataStart >= lines.length) throw new Error('No data found');

  // Parse column header
  const header = lines[dataStart].split(';');
  dataStart++;

  // Detect columns: first is time_s, rest are channels
  const hasTimeCol = header[0].toLowerCase().startsWith('time');
  const chanStart = hasTimeCol ? 1 : 0;

  // Parse channel names and types from header like "pin.name[FLOAT]"
  const chanNames: string[] = [];
  const chanTypes: number[] = [];
  const typeMap: Record<string, number> = { BIT: 1, FLOAT: 2, S32: 3, U32: 4 };
  for (let i = chanStart; i < header.length; i++) {
    const col = header[i].trim();
    const tm = col.match(/^(.+)\[(\w+)\]$/);
    if (tm) {
      chanNames.push(tm[1]);
      chanTypes.push(typeMap[tm[2]] ?? 2);
    } else {
      chanNames.push(col);
      chanTypes.push(2); // default FLOAT
    }
  }

  const sampleCount = lines.length - dataStart;
  if (sampleCount === 0) throw new Error('No sample rows');

  // Parse data
  const timeArr = new Float64Array(sampleCount);
  const chanData: Float64Array[] = chanNames.map(() => new Float64Array(sampleCount));

  for (let si = 0; si < sampleCount; si++) {
    const cols = lines[dataStart + si].split(';');
    if (hasTimeCol) {
      timeArr[si] = Number(cols[0]);
    }
    for (let ci = 0; ci < chanNames.length; ci++) {
      chanData[ci][si] = Number(cols[chanStart + ci]);
    }
  }

  // If no time column, reconstruct from period
  if (!hasTimeCol && periodNs > 0) {
    const dt = periodNs / 1e9;
    for (let i = 0; i < sampleCount; i++) {
      timeArr[i] = i * dt;
    }
  }

  // Update store with loaded data
  const samples: ChannelSamples[] = [];
  const channels = [];
  for (let ci = 0; ci < chanNames.length; ci++) {
    samples.push({ channel: ci, data: chanData[ci] });
    channels.push({
      channel: ci,
      pinName: chanNames[ci],
      dataType: chanTypes[ci],
      enabled: true,
    });
  }

  state.samples = samples;
  state.timeBase = timeArr;
  state.status.channels = channels;
  state.status.maxChannels = Math.max(chanNames.length, state.status.maxChannels);
  state.status.recLen = sampleCount;
  state.status.state = ScopeState.DONE;
  state.selectedChannel = 0;
  state.error = '';
}

function formatTimeValue(seconds: number): string {
  const sign = seconds < 0 ? '-' : '';
  let val = Math.abs(seconds) * 1e9; // to nanoseconds
  let units = 'ns';
  if (val >= 1000) { val /= 1000; units = 'µs'; }
  if (val >= 1000) { val /= 1000; units = 'ms'; }
  if (val >= 1000) { val /= 1000; units = 's'; }
  const decimals = val >= 100 ? 0 : val >= 10 ? 1 : 2;
  return `${sign}${val.toFixed(decimals)} ${units}`;
}

// --- Exported store ---

export const scopeStore = {
  state,
  connect,
  disconnect,
  configure,
  addChannel,
  removeChannel,
  setTrigger,
  arm,
  run,
  stop,
  fullReset,
  forceTrigger,
  searchPins,
  applyConfig,
  isCapturing,

  // Mutable config refs for v-model binding
  captureConfig: state.captureConfig,
  triggerConfig: state.triggerConfig,
  channelUI: state.channelUI,

  // Direct state mutation helpers
  setSelectedThread(name: string) {
    state.selectedThread = name;
  },
  setPinFilter(f: string) {
    state.pinFilter = f;
  },
  setHorizZoom,
  setHorizPos,
  setSelectedChannel(ch: number) {
    state.selectedChannel = ch;
  },
  calcDispScale,
  calcDisplayWindow,
  getSamplePeriod,
  formatTimeValue,
  saveCapture,
  loadCapture,
};
