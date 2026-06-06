<script setup lang="ts">
import { ref, watch } from 'vue';
import { modbusStore } from '../stores/modbus';
import { ModbusReqType, type ModbusRequest } from '../generated/classicladder_client';

const requests = ref<ModbusRequest[]>([]);

watch(() => modbusStore.state.requests, (reqs) => {
  requests.value = reqs.map(r => ({ ...r }));
}, { immediate: true });

const reqTypeOptions = [
  { value: ModbusReqType.COIL_READ, label: 'Read Coils (FC1)' },
  { value: ModbusReqType.COIL_WRITE, label: 'Write Coils (FC5/15)' },
  { value: ModbusReqType.INPUT_READ, label: 'Read Inputs (FC2)' },
  { value: ModbusReqType.HOLDING_READ, label: 'Read Holdings (FC3)' },
  { value: ModbusReqType.REGISTER_WRITE, label: 'Write Registers (FC6/16)' },
  { value: ModbusReqType.DIAGNOSTIC, label: 'Diagnostics (FC8)' },
];

function addRow() {
  requests.value.push(modbusStore.defaultRequest());
}

function removeRow(index: number) {
  requests.value.splice(index, 1);
}

function save() {
  modbusStore.saveRequests([...requests.value]);
}
</script>

<template>
  <div class="requests-panel">
    <div class="toolbar">
      <h2>Modbus I/O Requests</h2>
      <button class="btn" @click="addRow">+ Add</button>
    </div>

    <table v-if="requests.length > 0">
      <thead>
        <tr>
          <th>#</th>
          <th>Slave Address</th>
          <th>Function</th>
          <th>First Element</th>
          <th>Count</th>
          <th>Var Offset</th>
          <th>Inv</th>
          <th></th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="(req, i) in requests" :key="i">
          <td class="row-num">{{ i + 1 }}</td>
          <td><input v-model="req.slaveAddr" /></td>
          <td>
            <select v-model.number="req.typeReq">
              <option v-for="opt in reqTypeOptions" :key="opt.value" :value="opt.value">
                {{ opt.label }}
              </option>
            </select>
          </td>
          <td><input type="number" v-model.number="req.firstModbusElement" min="0" /></td>
          <td><input type="number" v-model.number="req.nbrModbusElements" min="1" /></td>
          <td><input type="number" v-model.number="req.offsetVarMapped" min="0" /></td>
          <td class="center"><input type="checkbox" v-model="req.logicInverted" /></td>
          <td><button class="btn btn-danger btn-sm" @click="removeRow(i)">✕</button></td>
        </tr>
      </tbody>
    </table>

    <p v-else class="empty">No requests configured. Click "+ Add" to create one.</p>

    <div class="actions">
      <button class="btn" @click="save">Apply</button>
    </div>
  </div>
</template>

<style scoped>
h2 {
  font-size: 14px;
  font-weight: 600;
  color: #89b4fa;
  margin: 0;
}

.toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
}

table {
  width: 100%;
  border-collapse: collapse;
}

th, td {
  padding: 6px 8px;
  text-align: left;
  border-bottom: 1px solid #45475a;
}

th {
  font-size: 11px;
  text-transform: uppercase;
  color: #a6adc8;
  font-weight: 500;
}

td input, td select {
  width: 100%;
}

td.row-num {
  width: 30px;
  text-align: center;
  color: #a6adc8;
}

td.center {
  text-align: center;
}

td.center input[type="checkbox"] {
  width: auto;
}

.btn-sm {
  padding: 4px 8px;
  font-size: 11px;
}

.empty {
  color: #a6adc8;
  text-align: center;
  padding: 24px;
}

.actions {
  margin-top: 16px;
  display: flex;
  justify-content: flex-end;
}
</style>
