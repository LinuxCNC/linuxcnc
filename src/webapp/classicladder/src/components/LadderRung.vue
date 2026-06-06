<script setup lang="ts">
import { computed, ref } from 'vue';
import type { Rung, Element } from '../generated/classicladder_client';
import { ladderStore, elementSize } from '../stores/ladder';

const props = defineProps<{
  rung: Rung;
  rungIndex: number;
  symbols?: Map<string, string>;
}>();

const emit = defineEmits<{
  (e: 'cellClick', row: number, col: number): void;
}>();

const COLS = 10;
const ROWS = 6;
const CELL_W = 80;
const CELL_H = 40;
const RAIL_W = 4;

// Element type constants
const ELE_FREE = 0;
const ELE_INPUT = 1;
const ELE_INPUT_NOT = 2;
const ELE_RISING_INPUT = 3;
const ELE_FALLING_INPUT = 4;
const ELE_CONNECTION = 9;
const ELE_TIMER = 10;
const ELE_MONOSTABLE = 11;
const ELE_COUNTER = 12;
const ELE_TIMER_IEC = 13;
const ELE_COMPAR = 20;
const ELE_OUTPUT = 50;
const ELE_OUTPUT_NOT = 51;
const ELE_OUTPUT_SET = 52;
const ELE_OUTPUT_RESET = 53;
const ELE_OUTPUT_JUMP = 54;
const ELE_OUTPUT_CALL = 55;
const ELE_OUTPUT_OPERATE = 60;
const ELE_BLOCK_BODY = 99;

// Variable type constants
const VAR_MEM_BIT = 0;
const VAR_TIMER_DONE = 10;
const VAR_TIMER_RUNNING = 11;
const VAR_TIMER_IEC_DONE = 15;
const VAR_MONOSTABLE_RUNNING = 20;
const VAR_COUNTER_DONE = 25;
const VAR_COUNTER_EMPTY = 26;
const VAR_COUNTER_FULL = 27;
const VAR_STEP_ACTIVITY = 30;
const VAR_PHYS_INPUT = 50;
const VAR_PHYS_OUTPUT = 60;
const VAR_ERROR_BIT = 70;

function getElement(row: number, col: number): Element {
  const idx = row * COLS + col;
  if (props.rung.elements && idx < props.rung.elements.length) {
    return props.rung.elements[idx];
  }
  return { type: 0, connectedWithTop: 0, varType: 0, varNum: 0 };
}

function varPrefix(varType: number): string {
  switch (varType) {
    case VAR_MEM_BIT: return '%B';
    case VAR_TIMER_DONE: return '%TM';
    case VAR_TIMER_RUNNING: return '%TM';
    case VAR_TIMER_IEC_DONE: return '%TI';
    case VAR_MONOSTABLE_RUNNING: return '%M';
    case VAR_COUNTER_DONE: return '%C';
    case VAR_COUNTER_EMPTY: return '%C';
    case VAR_COUNTER_FULL: return '%C';
    case VAR_STEP_ACTIVITY: return '%X';
    case VAR_PHYS_INPUT: return '%I';
    case VAR_PHYS_OUTPUT: return '%Q';
    case VAR_ERROR_BIT: return '%E';
    default: return '%?';
  }
}

function varName(el: Element): string {
  if (el.type === ELE_TIMER) return `%T${el.varNum}`;
  if (el.type === ELE_MONOSTABLE) return `%M${el.varNum}`;
  if (el.type === ELE_COUNTER) return `%C${el.varNum}`;
  if (el.type === ELE_TIMER_IEC) return `%TI${el.varNum}`;
  return `${varPrefix(el.varType)}${el.varNum}`;
}

function symbolOrVar(el: Element): string {
  if (el.type === ELE_FREE || el.type === ELE_CONNECTION || el.type === ELE_BLOCK_BODY) return '';
  const name = varName(el);
  if (props.symbols?.has(name)) return props.symbols.get(name)!;
  return name;
}

function isContact(type: number): boolean {
  return type >= ELE_INPUT && type <= ELE_FALLING_INPUT;
}

function isCoil(type: number): boolean {
  return type >= ELE_OUTPUT && type <= ELE_OUTPUT_CALL;
}

function isBlock(type: number): boolean {
  return type === ELE_TIMER || type === ELE_MONOSTABLE ||
         type === ELE_COUNTER || type === ELE_TIMER_IEC;
}

function blockName(type: number): string {
  switch (type) {
    case ELE_TIMER: return 'Timer';
    case ELE_MONOSTABLE: return 'Mono';
    case ELE_COUNTER: return 'Counter';
    case ELE_TIMER_IEC: return 'Timer IEC';
    default: return '';
  }
}

// How many rows does a block span?
function blockRows(type: number): number {
  if (type === ELE_COUNTER) return 4;
  return 2; // Timer, Mono, TimerIEC
}

const svgWidth = computed(() => COLS * CELL_W + 2 * RAIL_W);
const svgHeight = computed(() => {
  let lastRow = 0;
  for (let r = 0; r < ROWS; r++) {
    for (let c = 0; c < COLS; c++) {
      if (getElement(r, c).type !== ELE_FREE) lastRow = r;
    }
  }
  return Math.max((lastRow + 2), 2) * CELL_H;
});

interface CellInfo { row: number; col: number; el: Element; }

const cells = computed((): CellInfo[] => {
  const result: CellInfo[] = [];
  const usedRows = svgHeight.value / CELL_H;
  for (let r = 0; r < usedRows; r++) {
    for (let c = 0; c < COLS; c++) {
      result.push({ row: r, col: c, el: getElement(r, c) });
    }
  }
  return result;
});

// Coordinate helpers — match GTK: x = left edge of cell, y = top edge of cell
// W = CELL_W, H = CELL_H. GTK uses Width/3, Width/4, Height/2, Height/3, Height/4
const W = CELL_W;
const H = CELL_H;
const W3 = Math.round(W / 3);
const W4 = Math.round(W / 4);
const H2 = Math.round(H / 2);
const H3 = Math.round(H / 3);
const H4 = Math.round(H / 4);

function cx(col: number): number { return RAIL_W + col * W; }
function cy(row: number): number { return row * H; }
function cmy(row: number): number { return cy(row) + H2; }

const hoverCell = ref<{ row: number; col: number } | null>(null);

function onHover(row: number, col: number) {
  hoverCell.value = { row, col };
}
function onLeave() {
  hoverCell.value = null;
}

const hoverPreview = computed(() => {
  if (!hoverCell.value) return null;
  const tool = ladderStore.state.editTool;
  if (tool < 0) return null; // no tool selected
  const sz = elementSize(tool);
  const { row, col } = hoverCell.value;
  // Bounds check
  if (col + sz.cols > COLS || row + sz.rows > ROWS) return null;
  return { x: cx(col), y: cy(row), w: sz.cols * W, h: sz.rows * H };
});
</script>

<template>
  <div class="rung">
    <div class="rung-header">
      <span class="rung-num">{{ rungIndex }}</span>
      <span class="rung-label" v-if="rung.label">{{ rung.label }}</span>
      <span class="rung-comment" v-if="rung.comment">{{ rung.comment }}</span>
    </div>
    <svg :width="svgWidth" :height="svgHeight + 8" :viewBox="`0 -8 ${svgWidth} ${svgHeight + 8}`" class="rung-svg">
      <!-- Power rails -->
      <line :x1="RAIL_W/2" y1="-8" :x2="RAIL_W/2" :y2="svgHeight" class="rail"/>
      <line :x1="svgWidth - RAIL_W/2" y1="-8" :x2="svgWidth - RAIL_W/2" :y2="svgHeight" class="rail"/>

      <template v-for="cell in cells" :key="`${cell.row}-${cell.col}`">
        <!-- Vertical top-connection: at LEFT edge of cell, from cmy(row) up to cmy(row-1) -->
        <line v-if="cell.el.connectedWithTop && cell.row > 0 && cell.el.type !== ELE_BLOCK_BODY"
              :x1="cx(cell.col)" :y1="cmy(cell.row)"
              :x2="cx(cell.col)" :y2="cmy(cell.row - 1)" class="wire"/>

        <!-- Connection (horizontal wire through full cell) -->
        <line v-if="cell.el.type === ELE_CONNECTION"
              :x1="cx(cell.col)" :y1="cmy(cell.row)"
              :x2="cx(cell.col) + W" :y2="cmy(cell.row)" class="wire"/>

        <!-- Contacts: bars at W/3 and 2*W/3, wires from edges to bars -->
        <g v-if="isContact(cell.el.type)" class="clickable">
          <!-- Horizontal wires -->
          <line :x1="cx(cell.col)" :y1="cmy(cell.row)"
                :x2="cx(cell.col) + W3" :y2="cmy(cell.row)" class="wire"/>
          <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row)"
                :x2="cx(cell.col) + W" :y2="cmy(cell.row)" class="wire"/>
          <!-- Vertical bars -->
          <line :x1="cx(cell.col) + W3" :y1="cy(cell.row) + H4"
                :x2="cx(cell.col) + W3" :y2="cy(cell.row) + H - H4" class="contact-bar"/>
          <line :x1="cx(cell.col) + 2*W3" :y1="cy(cell.row) + H4"
                :x2="cx(cell.col) + 2*W3" :y2="cy(cell.row) + H - H4" class="contact-bar"/>
          <!-- Negation slash -->
          <line v-if="cell.el.type === ELE_INPUT_NOT"
                :x1="cx(cell.col) + W3" :y1="cy(cell.row) + H - H4"
                :x2="cx(cell.col) + 2*W3" :y2="cy(cell.row) + H4" class="negation"/>
          <!-- Rising/Falling edge markers -->
          <g v-if="cell.el.type === ELE_RISING_INPUT">
            <line :x1="cx(cell.col) + W3" :y1="cy(cell.row) + 2*H3"
                  :x2="cx(cell.col) + W/2" :y2="cy(cell.row) + H3" class="contact-bar"/>
            <line :x1="cx(cell.col) + W/2" :y1="cy(cell.row) + H3"
                  :x2="cx(cell.col) + 2*W3" :y2="cy(cell.row) + 2*H3" class="contact-bar"/>
          </g>
          <g v-if="cell.el.type === ELE_FALLING_INPUT">
            <line :x1="cx(cell.col) + W3" :y1="cy(cell.row) + H3"
                  :x2="cx(cell.col) + W/2" :y2="cy(cell.row) + 2*H3" class="contact-bar"/>
            <line :x1="cx(cell.col) + W/2" :y1="cy(cell.row) + 2*H3"
                  :x2="cx(cell.col) + 2*W3" :y2="cy(cell.row) + H3" class="contact-bar"/>
          </g>
          <!-- Variable name label (above, clear of element) -->
          <text :x="cx(cell.col) + W/2" :y="cy(cell.row) + H4 - 4" class="lbl contact-lbl">{{ symbolOrVar(cell.el) }}</text>
        </g>

        <!-- Coils: arcs at W/4 to 3W/4, wires from edges -->
        <g v-else-if="isCoil(cell.el.type)" class="clickable">
          <!-- Horizontal wires (stop at arc edges W/4 and 3W/4) -->
          <line :x1="cx(cell.col)" :y1="cmy(cell.row)"
                :x2="cx(cell.col) + W4" :y2="cmy(cell.row)" class="wire"/>
          <line :x1="cx(cell.col) + W - W4" :y1="cmy(cell.row)"
                :x2="cx(cell.col) + W" :y2="cmy(cell.row)" class="wire"/>
          <!-- Coil parentheses as arcs -->
          <path :d="`M ${cx(cell.col)+W4+W4/2} ${cy(cell.row)+H4} A ${W4/2} ${H4} 0 0 0 ${cx(cell.col)+W4+W4/2} ${cy(cell.row)+H-H4}`" class="coil-arc"/>
          <path :d="`M ${cx(cell.col)+W-W4-W4/2} ${cy(cell.row)+H4} A ${W4/2} ${H4} 0 0 1 ${cx(cell.col)+W-W4-W4/2} ${cy(cell.row)+H-H4}`" class="coil-arc"/>
          <!-- S/R/J/C letter -->
          <text v-if="cell.el.type === ELE_OUTPUT_NOT" :x="cx(cell.col)+W/2" :y="cmy(cell.row)" class="coil-t">/</text>
          <text v-if="cell.el.type === ELE_OUTPUT_SET" :x="cx(cell.col)+W/2" :y="cmy(cell.row)" class="coil-t">S</text>
          <text v-if="cell.el.type === ELE_OUTPUT_RESET" :x="cx(cell.col)+W/2" :y="cmy(cell.row)" class="coil-t">R</text>
          <text v-if="cell.el.type === ELE_OUTPUT_JUMP" :x="cx(cell.col)+W/2" :y="cmy(cell.row)" class="coil-t">J</text>
          <text v-if="cell.el.type === ELE_OUTPUT_CALL" :x="cx(cell.col)+W/2" :y="cmy(cell.row)" class="coil-t">C</text>
          <!-- Variable name label -->
          <text :x="cx(cell.col) + W/2" :y="cy(cell.row) + H4 - 4" class="lbl coil-lbl">{{ symbolOrVar(cell.el) }}</text>
        </g>

        <!-- Timer/Monostable/Counter/TimerIEC blocks
             Head element is at the RIGHT column of the 2-col block.
             Block box extends LEFT by one cell width from the head. -->
        <g v-else-if="isBlock(cell.el.type)" class="clickable">
          <!-- Box: x from cx(col)+W/3-W, width W+W/3, height depends on block type -->
          <rect :x="cx(cell.col) + W3 - W" :y="cy(cell.row) + H3"
                :width="W + W3" :height="blockRows(cell.el.type) === 4 ? 3*H + H3 : H + H3" class="block-rect"/>
          <!-- Block title text (centered in box: box starts at cy+H3, height H+H3 for timer) -->
          <text :x="cx(cell.col) + W3 - W + (W+W3)/2" :y="cy(cell.row) + H3 + (blockRows(cell.el.type) === 4 ? (3*H+H3)/2 - 8 : (H+H3)/2 - 5)" class="blk-title">{{ blockName(cell.el.type) }}</text>
          <!-- Variable/symbol name -->
          <text :x="cx(cell.col) + W3 - W + (W+W3)/2" :y="cy(cell.row) + H3 + (blockRows(cell.el.type) === 4 ? (3*H+H3)/2 + 6 : (H+H3)/2 + 7)" class="blk-var">{{ symbolOrVar(cell.el) }}</text>

          <!-- Timer/Mono: E input, C input, D output, R output -->
          <template v-if="cell.el.type === ELE_TIMER || cell.el.type === ELE_MONOSTABLE">
            <!-- E input wire + label -->
            <line :x1="cx(cell.col) - W" :y1="cmy(cell.row)"
                  :x2="cx(cell.col) - W + W3" :y2="cmy(cell.row)" class="wire"/>
            <!-- E input wire + label -->
            <line :x1="cx(cell.col) - W" :y1="cmy(cell.row)"
                  :x2="cx(cell.col) - W + W3" :y2="cmy(cell.row)" class="wire"/>
            <text :x="cx(cell.col) - W + W3 + 4" :y="cmy(cell.row)" class="term">E</text>
            <!-- C input wire + label -->
            <line :x1="cx(cell.col) - W" :y1="cmy(cell.row + 1)"
                  :x2="cx(cell.col) - W + W3" :y2="cmy(cell.row + 1)" class="wire"/>
            <text :x="cx(cell.col) - W + W3 + 4" :y="cmy(cell.row + 1)" class="term">C</text>
            <!-- D output wire + label -->
            <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row)"
                  :x2="cx(cell.col) + W" :y2="cmy(cell.row)" class="wire"/>
            <text :x="cx(cell.col) + 2*W3 - 4" :y="cmy(cell.row)" class="term-r">D</text>
            <!-- R output wire + label -->
            <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row + 1)"
                  :x2="cx(cell.col) + W" :y2="cmy(cell.row + 1)" class="wire"/>
            <text :x="cx(cell.col) + 2*W3 - 4" :y="cmy(cell.row + 1)" class="term-r">R</text>
          </template>

          <!-- Timer IEC: I input, Q output (single row of I/O) -->
          <template v-if="cell.el.type === ELE_TIMER_IEC">
            <line :x1="cx(cell.col) - W" :y1="cmy(cell.row)"
                  :x2="cx(cell.col) - W + W3" :y2="cmy(cell.row)" class="wire"/>
            <text :x="cx(cell.col) - W + W3 + 4" :y="cmy(cell.row)" class="term">I</text>
            <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row)"
                  :x2="cx(cell.col) + W" :y2="cmy(cell.row)" class="wire"/>
            <text :x="cx(cell.col) + 2*W3 - 4" :y="cmy(cell.row)" class="term-r">Q</text>
          </template>

          <!-- Counter: R/P/U/D inputs at rows 0-3, E/D/F outputs at rows 0-2 -->
          <template v-if="cell.el.type === ELE_COUNTER">
            <line :x1="cx(cell.col) - W" :y1="cmy(cell.row)"
                  :x2="cx(cell.col) - W + W3" :y2="cmy(cell.row)" class="wire"/>
            <text :x="cx(cell.col) - W + W3 + 4" :y="cmy(cell.row)" class="term">R</text>
            <line :x1="cx(cell.col) - W" :y1="cmy(cell.row + 1)"
                  :x2="cx(cell.col) - W + W3" :y2="cmy(cell.row + 1)" class="wire"/>
            <text :x="cx(cell.col) - W + W3 + 4" :y="cmy(cell.row + 1)" class="term">P</text>
            <line :x1="cx(cell.col) - W" :y1="cmy(cell.row + 2)"
                  :x2="cx(cell.col) - W + W3" :y2="cmy(cell.row + 2)" class="wire"/>
            <text :x="cx(cell.col) - W + W3 + 4" :y="cmy(cell.row + 2)" class="term">U</text>
            <line :x1="cx(cell.col) - W" :y1="cmy(cell.row + 3)"
                  :x2="cx(cell.col) - W + W3" :y2="cmy(cell.row + 3)" class="wire"/>
            <text :x="cx(cell.col) - W + W3 + 4" :y="cmy(cell.row + 3)" class="term">D</text>
            <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row)"
                  :x2="cx(cell.col) + W" :y2="cmy(cell.row)" class="wire"/>
            <text :x="cx(cell.col) + 2*W3 - 4" :y="cmy(cell.row)" class="term-r">E</text>
            <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row + 1)"
                  :x2="cx(cell.col) + W" :y2="cmy(cell.row + 1)" class="wire"/>
            <text :x="cx(cell.col) + 2*W3 - 4" :y="cmy(cell.row + 1)" class="term-r">D</text>
            <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row + 2)"
                  :x2="cx(cell.col) + W" :y2="cmy(cell.row + 2)" class="wire"/>
            <text :x="cx(cell.col) + 2*W3 - 4" :y="cmy(cell.row + 2)" class="term-r">F</text>
          </template>
        </g>

        <!-- Compare: spans 3 cells wide (extends 2 cells LEFT from head) -->
        <g v-else-if="cell.el.type === ELE_COMPAR" class="clickable">
          <rect :x="cx(cell.col) + W3 - 2*W" :y="cy(cell.row) + H4"
                :width="2*W + W3" :height="2*H4" class="block-rect"/>
          <line :x1="cx(cell.col) - 2*W" :y1="cmy(cell.row)"
                :x2="cx(cell.col) - 2*W + W3" :y2="cmy(cell.row)" class="wire"/>
          <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row)"
                :x2="cx(cell.col) + W" :y2="cmy(cell.row)" class="wire"/>
          <text :x="cx(cell.col) + W3 - 2*W + 4" :y="cy(cell.row) + H4 + 2" class="term">COMPARISON</text>
          <text :x="cx(cell.col) + W3 - 2*W + (2*W+W3)/2" :y="cmy(cell.row) + 4" class="blk-lbl">{{ symbolOrVar(cell.el) }}</text>
        </g>

        <!-- Operate output: spans 3 cells wide (extends 2 cells LEFT from head) -->
        <g v-else-if="cell.el.type === ELE_OUTPUT_OPERATE" class="clickable">
          <rect :x="cx(cell.col) + W3 - 2*W" :y="cy(cell.row) + H4"
                :width="2*W + W3" :height="2*H4" class="block-rect"/>
          <line :x1="cx(cell.col) - 2*W" :y1="cmy(cell.row)"
                :x2="cx(cell.col) - 2*W + W3" :y2="cmy(cell.row)" class="wire"/>
          <line :x1="cx(cell.col) + 2*W3" :y1="cmy(cell.row)"
                :x2="cx(cell.col) + W" :y2="cmy(cell.row)" class="wire"/>
          <text :x="cx(cell.col) + W3 - 2*W + 4" :y="cy(cell.row) + H4 + 2" class="term">ASSIGNMENT</text>
          <text :x="cx(cell.col) + W3 - 2*W + (2*W+W3)/2" :y="cmy(cell.row) + 4" class="blk-lbl">{{ symbolOrVar(cell.el) }}</text>
        </g>
      </template>

      <!-- Clickable grid overlay for editing -->
      <template v-for="cell in cells" :key="`click-${cell.row}-${cell.col}`">
        <rect :x="cx(cell.col)" :y="cy(cell.row)" :width="W" :height="H"
              class="click-overlay"
              @click="emit('cellClick', cell.row, cell.col)"
              @mouseenter="onHover(cell.row, cell.col)"
              @mouseleave="onLeave()"/>
      </template>

      <!-- Hover preview showing footprint of element to be placed -->
      <rect v-if="hoverPreview"
            :x="hoverPreview.x" :y="hoverPreview.y"
            :width="hoverPreview.w" :height="hoverPreview.h"
            class="hover-preview"/>
    </svg>
  </div>
</template>

<style scoped>
.rung { margin-bottom: 8px; border: 1px solid #45475a; border-radius: 4px; overflow: hidden; }
.rung-header { display: flex; align-items: center; gap: 8px; padding: 4px 8px; background: #1e1e2e; border-bottom: 1px solid #45475a; font-size: 11px; }
.rung-num { font-weight: 700; color: #89b4fa; min-width: 24px; }
.rung-label { color: #a6e3a1; font-weight: 600; }
.rung-comment { color: #a6adc8; font-style: italic; }
.rung-svg { display: block; background: #181825; }
.rail { stroke: #89b4fa; stroke-width: 3; }
.wire { stroke: #9399b2; stroke-width: 1.5; }
.contact-bar { stroke: #a6e3a1; stroke-width: 2; }
.negation { stroke: #f38ba8; stroke-width: 1.5; }
.coil-arc { fill: none; stroke: #f9e2af; stroke-width: 1.5; }
.coil-t { fill: #f9e2af; font-size: 10px; font-weight: 700; text-anchor: middle; dominant-baseline: middle; }
.lbl { font-size: 9px; text-anchor: middle; fill: #a6adc8; }
.contact-lbl { fill: #f38ba8; }
.coil-lbl { fill: #89b4fa; }
.block-rect { fill: #313244; stroke: #9399b2; stroke-width: 1; rx: 2; }
.blk-title { fill: #cba6f7; font-size: 10px; font-weight: 700; text-anchor: middle; }
.blk-var { fill: #a6adc8; font-size: 9px; text-anchor: middle; }
.blk-lbl { fill: #a6adc8; font-size: 9px; text-anchor: middle; dominant-baseline: middle; }
.term { fill: #a6adc8; font-size: 8px; text-anchor: start; dominant-baseline: middle; }
.term-r { fill: #a6adc8; font-size: 8px; text-anchor: end; dominant-baseline: middle; }
.clickable { cursor: pointer; }
.clickable:hover .wire { stroke: #89b4fa; }
.clickable:hover .contact-bar { stroke: #b5f0c7; }
.clickable:hover .coil-arc { stroke: #fce8b2; }
.click-overlay { fill: transparent; cursor: pointer; }
.click-overlay:hover { fill: rgba(137, 180, 250, 0.03); }
.hover-preview { fill: rgba(137, 180, 250, 0.12); stroke: #89b4fa; stroke-width: 1; stroke-dasharray: 3 2; pointer-events: none; }
</style>
