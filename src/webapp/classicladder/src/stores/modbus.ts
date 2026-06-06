import { reactive } from 'vue';
import {
  ClassicladderClient,
  type ModbusComParams,
  type ModbusRequest,
  type ModbusStatus,
  ModbusReqType,
} from '../generated/classicladder_client';

export interface ModbusStoreState {
  comParams: ModbusComParams | null;
  requests: ModbusRequest[];
  status: ModbusStatus | null;
  loading: boolean;
  error: string;
  pollTimer: ReturnType<typeof setInterval> | null;
}

const client = new ClassicladderClient(window.location.origin);

const state = reactive<ModbusStoreState>({
  comParams: null,
  requests: [],
  status: null,
  loading: false,
  error: '',
  pollTimer: null,
});

async function fetchAll() {
  state.loading = true;
  state.error = '';
  try {
    const [params, reqs, status] = await Promise.all([
      client.getModbusComParams(),
      client.getModbusRequests(),
      client.getModbusStatus(),
    ]);
    state.comParams = params;
    state.requests = reqs;
    state.status = status;
  } catch (e: unknown) {
    state.error = (e as Error).message;
  } finally {
    state.loading = false;
  }
}

async function saveComParams(params: ModbusComParams) {
  state.error = '';
  try {
    await client.setModbusComParams(params);
    state.comParams = { ...params };
  } catch (e: unknown) {
    state.error = (e as Error).message;
  }
}

async function saveRequests(requests: ModbusRequest[]) {
  state.error = '';
  try {
    await client.setModbusRequests(requests);
    state.requests = [...requests];
  } catch (e: unknown) {
    state.error = (e as Error).message;
  }
}

async function refreshStatus() {
  try {
    state.status = await client.getModbusStatus();
  } catch {
    // silently ignore status poll errors
  }
}

function startPolling(intervalMs = 1000) {
  stopPolling();
  state.pollTimer = setInterval(refreshStatus, intervalMs);
}

function stopPolling() {
  if (state.pollTimer) {
    clearInterval(state.pollTimer);
    state.pollTimer = null;
  }
}

function defaultComParams(): ModbusComParams {
  return {
    serialPort: '/dev/ttyS0',
    serialSpeed: 9600,
    serialDataBits: 8,
    serialStopBits: 1,
    serialParity: 0,
    serialUseRts: false,
    elementOffset: 1,
    timeInterFrame: 100,
    timeOutReceipt: 500,
    timeAfterTransmit: 0,
    debugLevel: 0,
    mapCoilRead: 1,
    mapCoilWrite: 0,
    mapInputs: 0,
    mapHolding: 0,
    mapRegisterRead: 1,
    mapRegisterWrite: 0,
  };
}

function defaultRequest(): ModbusRequest {
  return {
    slaveAddr: '1',
    typeReq: ModbusReqType.COIL_READ,
    firstModbusElement: 1,
    nbrModbusElements: 1,
    logicInverted: false,
    offsetVarMapped: 0,
  };
}

export const modbusStore = {
  state,
  fetchAll,
  saveComParams,
  saveRequests,
  refreshStatus,
  startPolling,
  stopPolling,
  defaultComParams,
  defaultRequest,
};
