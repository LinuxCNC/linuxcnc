import { reactive } from 'vue';
import {
  ClassicladderClient,
  type Program,
  LadderState,
} from '../generated/classicladder_client';

export interface LadderStoreState {
  program: Program | null;
  activeSection: number;
  editTool: number; // element type to place, 0 = delete
  selectedCell: { rungIdx: number; row: number; col: number } | null;
  dirty: boolean;
  symbolMap: Map<string, string>;
  loading: boolean;
  error: string;
}

const COLS = 10;

const client = new ClassicladderClient(window.location.origin);

const state = reactive<LadderStoreState>({
  program: null,
  activeSection: 0,
  editTool: -1, // -1 = no tool selected
  selectedCell: null,
  dirty: false,
  symbolMap: new Map(),
  loading: false,
  error: '',
});

async function fetchProgram() {
  state.loading = true;
  state.error = '';
  try {
    state.program = await client.getProgram();
    buildSymbolMap();
    state.dirty = false;
    // Set active section to first used section
    if (state.program.sections) {
      const firstUsed = state.program.sections.findIndex(s => s.used);
      if (firstUsed >= 0) state.activeSection = firstUsed;
    }
  } catch (e: unknown) {
    state.error = (e as Error).message;
  } finally {
    state.loading = false;
  }
}

function buildSymbolMap() {
  state.symbolMap.clear();
  if (!state.program?.symbols) return;
  for (const sym of state.program.symbols) {
    if (sym.varName && sym.symbol) {
      // varName is like "%B0", "%I3", etc.
      state.symbolMap.set(sym.varName, sym.symbol);
    }
  }
}

function setActiveSection(index: number) {
  state.activeSection = index;
}

function setEditTool(type: number) {
  state.editTool = type;
}

const ROWS = 6;
const ELE_FREE = 0;
const ELE_BLOCK_BODY = 99;
const ELE_TIMER = 10;
const ELE_MONOSTABLE = 11;
const ELE_COUNTER = 12;
const ELE_TIMER_IEC = 13;
const ELE_COMPAR = 20;
const ELE_OUTPUT_OPERATE = 60;

// Returns { cols, rows } footprint of an element type
export function elementSize(type: number): { cols: number; rows: number } {
  switch (type) {
    case ELE_TIMER: case ELE_MONOSTABLE: case ELE_TIMER_IEC: return { cols: 2, rows: 2 };
    case ELE_COUNTER: return { cols: 2, rows: 4 };
    case ELE_COMPAR: case ELE_OUTPUT_OPERATE: return { cols: 3, rows: 1 };
    default: return { cols: 1, rows: 1 };
  }
}

function getEl(rung: { elements: { type: number; connectedWithTop: number; varType: number; varNum: number }[] }, row: number, col: number) {
  if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return null;
  return rung.elements[row * COLS + col] ?? null;
}

// Find the head element of a block that occupies (row, col).
// For type 99 body cells, searches nearby cells for the head.
function findBlockHead(rung: { elements: { type: number; connectedWithTop: number; varType: number; varNum: number }[] }, row: number, col: number): { row: number; col: number; type: number } | null {
  const el = getEl(rung, row, col);
  if (!el) return null;
  if (el.type !== ELE_BLOCK_BODY) {
    const sz = elementSize(el.type);
    if (sz.cols > 1 || sz.rows > 1) return { row, col, type: el.type };
    return null;
  }
  // Search for the head: head is at top-right of block (for 2-col blocks)
  // or rightmost cell (for 3-col blocks). Scan nearby cells.
  for (let r = Math.max(0, row - 3); r <= row; r++) {
    for (let c = col; c <= Math.min(COLS - 1, col + 2); c++) {
      const candidate = getEl(rung, r, c);
      if (!candidate) continue;
      const sz = elementSize(candidate.type);
      if (sz.cols <= 1 && sz.rows <= 1) continue;
      // Check if (row, col) falls within this block's footprint
      // Head is at (r, c), footprint extends left by (sz.cols-1) and down by (sz.rows-1)
      const leftCol = c - (sz.cols - 1);
      if (col >= leftCol && col <= c && row >= r && row < r + sz.rows) {
        return { row: r, col: c, type: candidate.type };
      }
    }
  }
  return null;
}

// Clear a single cell
function clearCell(rung: { elements: { type: number; connectedWithTop: number; varType: number; varNum: number }[] }, row: number, col: number) {
  const el = getEl(rung, row, col);
  if (!el) return;
  el.type = ELE_FREE;
  el.connectedWithTop = 0;
  el.varType = 0;
  el.varNum = 0;
}

// Remove an entire block (head + all body cells)
function removeBlock(rung: { elements: { type: number; connectedWithTop: number; varType: number; varNum: number }[] }, headRow: number, headCol: number, type: number) {
  const sz = elementSize(type);
  const leftCol = headCol - (sz.cols - 1);
  for (let r = headRow; r < headRow + sz.rows; r++) {
    for (let c = leftCol; c <= headCol; c++) {
      clearCell(rung, r, c);
    }
  }
}

// Clear all elements in a footprint area, removing any overlapping blocks fully
function clearFootprint(rung: { elements: { type: number; connectedWithTop: number; varType: number; varNum: number }[] }, topRow: number, leftCol: number, cols: number, rows: number) {
  for (let r = topRow; r < topRow + rows; r++) {
    for (let c = leftCol; c < leftCol + cols; c++) {
      const el = getEl(rung, r, c);
      if (!el || el.type === ELE_FREE) continue;
      if (el.type === ELE_BLOCK_BODY) {
        // Find and remove the parent block
        const head = findBlockHead(rung, r, c);
        if (head) removeBlock(rung, head.row, head.col, head.type);
      } else {
        const sz = elementSize(el.type);
        if (sz.cols > 1 || sz.rows > 1) {
          removeBlock(rung, r, c, el.type);
        } else {
          clearCell(rung, r, c);
        }
      }
    }
  }
}

function selectCell(rungIdx: number, row: number, col: number) {
  state.selectedCell = { rungIdx, row, col };

  if (state.editTool >= 0 && state.program) {
    const rung = state.program.rungs[rungIdx];
    if (!rung) return;

    if (state.editTool === ELE_FREE) {
      // Delete: if clicking on a block body, remove the whole block
      const el = getEl(rung, row, col);
      if (!el) return;
      if (el.type === ELE_BLOCK_BODY) {
        const head = findBlockHead(rung, row, col);
        if (head) removeBlock(rung, head.row, head.col, head.type);
      } else {
        const sz = elementSize(el.type);
        if (sz.cols > 1 || sz.rows > 1) {
          removeBlock(rung, row, col, el.type);
        } else {
          clearCell(rung, row, col);
        }
      }
    } else {
      const sz = elementSize(state.editTool);
      // For multi-cell blocks: click = top-left of footprint
      // Head goes at top-right (col + cols - 1) for blocks, rightmost for compare/operate
      const headCol = col + sz.cols - 1;
      const headRow = row;

      // Bounds check
      if (headCol >= COLS || row + sz.rows > ROWS) return;

      // Clear the footprint (removes overlapping blocks)
      clearFootprint(rung, row, col, sz.cols, sz.rows);

      // Place head
      const headEl = getEl(rung, headRow, headCol);
      if (!headEl) return;
      headEl.type = state.editTool;
      // For single-cell elements, we're done
      if (sz.cols > 1 || sz.rows > 1) {
        // Fill body cells with type 99
        for (let r = row; r < row + sz.rows; r++) {
          for (let c = col; c < col + sz.cols; c++) {
            if (r === headRow && c === headCol) continue;
            const bodyEl = getEl(rung, r, c);
            if (bodyEl) {
              bodyEl.type = ELE_BLOCK_BODY;
              bodyEl.varType = 0;
              bodyEl.varNum = 0;
              // Set connectedWithTop on cells below the top row of the block (left column)
              bodyEl.connectedWithTop = (r > row && c === col) ? 1 : 0;
            }
          }
        }
      }
    }
    state.dirty = true;
  }
}

function toggleTopConnection() {
  if (!state.selectedCell || !state.program) return;
  const { rungIdx, row, col } = state.selectedCell;
  const rung = state.program.rungs[rungIdx];
  if (!rung) return;
  const elIdx = row * COLS + col;
  if (elIdx >= rung.elements.length) return;
  rung.elements[elIdx].connectedWithTop = rung.elements[elIdx].connectedWithTop ? 0 : 1;
  state.dirty = true;
}

async function saveProgram() {
  if (!state.program || !state.dirty) return;
  state.error = '';
  try {
    await client.setProgram(state.program);
    state.dirty = false;
  } catch (e: unknown) {
    state.error = (e as Error).message;
  }
}

async function setState(s: LadderState) {
  try {
    await client.setState(s);
  } catch (e: unknown) {
    state.error = (e as Error).message;
  }
}

async function setVariable(varType: number, offset: number, value: number) {
  try {
    await client.setVariable(varType, offset, value);
  } catch (e: unknown) {
    state.error = (e as Error).message;
  }
}

export const ladderStore = {
  state,
  fetchProgram,
  setActiveSection,
  setEditTool,
  selectCell,
  toggleTopConnection,
  saveProgram,
  setState,
  setVariable,
};
