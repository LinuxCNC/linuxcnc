import { reactive } from 'vue';
import { ToolsClient, type ToolEntry } from '../generated/tools_client';

export interface ToolEditState {
  tools: ToolEntry[];
  loading: boolean;
  error: string | null;
}

const instance = new URLSearchParams(window.location.search).get('instance') || 'milltask';
const client = new ToolsClient(window.location.origin, instance);

const state = reactive<ToolEditState>({
  tools: [],
  loading: false,
  error: null,
});

async function loadTools() {
  state.loading = true;
  state.error = null;
  try {
    state.tools = await client.listTools();
  } catch (e: unknown) {
    state.error = e instanceof Error ? e.message : String(e);
  } finally {
    state.loading = false;
  }
}

async function saveTool(tool: ToolEntry) {
  state.error = null;
  try {
    await client.putTool(tool.toolno, tool);
    await loadTools();
  } catch (e: unknown) {
    state.error = e instanceof Error ? e.message : String(e);
  }
}

async function deleteTool(toolno: number) {
  state.error = null;
  try {
    await client.deleteTool(toolno);
    state.tools = state.tools.filter(t => t.toolno !== toolno);
  } catch (e: unknown) {
    state.error = e instanceof Error ? e.message : String(e);
  }
}

async function reloadTable() {
  state.error = null;
  try {
    await client.reloadTools();
    await loadTools();
  } catch (e: unknown) {
    state.error = e instanceof Error ? e.message : String(e);
  }
}

export const toolStore = {
  state,
  loadTools,
  saveTool,
  deleteTool,
  reloadTable,
};
