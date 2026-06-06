<script setup lang="ts">
import { reactive, watch } from 'vue';
import { modbusStore } from '../stores/modbus';
import type { ModbusComParams } from '../generated/classicladder_client';

const form = reactive<ModbusComParams>(modbusStore.defaultComParams());

// Sync form when data loads
watch(() => modbusStore.state.comParams, (params) => {
  if (params) Object.assign(form, params);
}, { immediate: true });

function save() {
  modbusStore.saveComParams({ ...form });
}

const parityOptions = [
  { value: 0, label: 'None' },
  { value: 1, label: 'Odd' },
  { value: 2, label: 'Even' },
];

const speedOptions = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200];
</script>

<template>
  <div class="com-params">
    <h2>Serial Port Configuration</h2>
    <div class="form-grid">
      <div class="field">
        <label>Serial Port</label>
        <input v-model="form.serialPort" placeholder="/dev/ttyS0" />
      </div>
      <div class="field">
        <label>Speed (baud)</label>
        <select v-model.number="form.serialSpeed">
          <option v-for="s in speedOptions" :key="s" :value="s">{{ s }}</option>
        </select>
      </div>
      <div class="field">
        <label>Data Bits</label>
        <select v-model.number="form.serialDataBits">
          <option :value="7">7</option>
          <option :value="8">8</option>
        </select>
      </div>
      <div class="field">
        <label>Stop Bits</label>
        <select v-model.number="form.serialStopBits">
          <option :value="1">1</option>
          <option :value="2">2</option>
        </select>
      </div>
      <div class="field">
        <label>Parity</label>
        <select v-model.number="form.serialParity">
          <option v-for="p in parityOptions" :key="p.value" :value="p.value">{{ p.label }}</option>
        </select>
      </div>
      <div class="field checkbox">
        <label><input type="checkbox" v-model="form.serialUseRts" /> Use RTS for transmit</label>
      </div>
    </div>

    <h2>Timing</h2>
    <div class="form-grid">
      <div class="field">
        <label>Inter-frame delay (ms)</label>
        <input type="number" v-model.number="form.timeInterFrame" min="0" />
      </div>
      <div class="field">
        <label>Receive timeout (ms)</label>
        <input type="number" v-model.number="form.timeOutReceipt" min="0" />
      </div>
      <div class="field">
        <label>After-transmit delay (ms)</label>
        <input type="number" v-model.number="form.timeAfterTransmit" min="0" />
      </div>
    </div>

    <h2>Variable Mapping</h2>
    <div class="form-grid">
      <div class="field">
        <label>Element offset</label>
        <input type="number" v-model.number="form.elementOffset" min="0" />
      </div>
      <div class="field">
        <label>Map coil read</label>
        <input type="number" v-model.number="form.mapCoilRead" min="0" />
      </div>
      <div class="field">
        <label>Map coil write</label>
        <input type="number" v-model.number="form.mapCoilWrite" min="0" />
      </div>
      <div class="field">
        <label>Map inputs</label>
        <input type="number" v-model.number="form.mapInputs" min="0" />
      </div>
      <div class="field">
        <label>Map holding</label>
        <input type="number" v-model.number="form.mapHolding" min="0" />
      </div>
      <div class="field">
        <label>Map register read</label>
        <input type="number" v-model.number="form.mapRegisterRead" min="0" />
      </div>
      <div class="field">
        <label>Map register write</label>
        <input type="number" v-model.number="form.mapRegisterWrite" min="0" />
      </div>
      <div class="field">
        <label>Debug level</label>
        <input type="number" v-model.number="form.debugLevel" min="0" max="3" />
      </div>
    </div>

    <div class="actions">
      <button class="btn" @click="save">Apply</button>
    </div>
  </div>
</template>

<style scoped>
h2 {
  font-size: 14px;
  font-weight: 600;
  margin: 16px 0 8px;
  color: #89b4fa;
}
h2:first-child {
  margin-top: 0;
}

.form-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
  gap: 12px;
}

.field.checkbox label {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 13px;
  color: #cdd6f4;
}

.field.checkbox input[type="checkbox"] {
  width: auto;
}

.actions {
  margin-top: 20px;
  display: flex;
  justify-content: flex-end;
}
</style>
