import { reactive } from 'vue';
import { EmccalibClient, type TunableSection, type TunableItem } from '../generated/emccalib_client';

const baseUrl = window.location.origin;
const client = new EmccalibClient(baseUrl);

interface CalibState {
  sections: TunableSection[];
  activeTab: string;
  loading: boolean;
  error: string;
  // Track pending edits: "SECTION\0KEY" → edited value string
  edits: Map<string, string>;
  saving: boolean;
  saveMessage: string;
}

const state = reactive<CalibState>({
  sections: [],
  activeTab: '',
  loading: false,
  error: '',
  edits: new Map(),
  saving: false,
  saveMessage: '',
});

function itemKey(section: string, key: string): string {
  return `${section}\0${key}`;
}

export const calibStore = {
  state,

  async loadTunables() {
    state.loading = true;
    state.error = '';
    try {
      state.sections = await client.getTunables();
      if (state.sections.length > 0 && !state.activeTab) {
        state.activeTab = state.sections[0].name;
      }
    } catch (e: unknown) {
      state.error = e instanceof Error ? e.message : String(e);
    } finally {
      state.loading = false;
    }
  },

  getEdit(section: string, key: string): string | undefined {
    return state.edits.get(itemKey(section, key));
  },

  setEdit(section: string, key: string, value: string) {
    state.edits.set(itemKey(section, key), value);
  },

  clearEdit(section: string, key: string) {
    state.edits.delete(itemKey(section, key));
  },

  async testValue(item: TunableItem) {
    const editVal = state.edits.get(itemKey(item.section, item.key));
    if (editVal === undefined) return;
    const numVal = parseFloat(editVal);
    if (isNaN(numVal)) {
      state.error = `Invalid number: ${editVal}`;
      return;
    }
    state.error = '';
    try {
      await client.setPin(item.section, item.key, numVal);
      state.edits.delete(itemKey(item.section, item.key));
      await this.loadTunables();
    } catch (e: unknown) {
      state.error = e instanceof Error ? e.message : String(e);
    }
  },

  async revertValue(item: TunableItem) {
    state.error = '';
    try {
      await client.revert(item.section, item.key);
      state.edits.delete(itemKey(item.section, item.key));
      await this.loadTunables();
    } catch (e: unknown) {
      state.error = e instanceof Error ? e.message : String(e);
    }
  },

  async saveAll() {
    state.saving = true;
    state.error = '';
    state.saveMessage = '';
    try {
      await client.saveIni();
      state.saveMessage = 'INI file saved successfully';
      await this.loadTunables();
    } catch (e: unknown) {
      state.error = e instanceof Error ? e.message : String(e);
    } finally {
      state.saving = false;
    }
  },

  setActiveTab(name: string) {
    state.activeTab = name;
  },
};
