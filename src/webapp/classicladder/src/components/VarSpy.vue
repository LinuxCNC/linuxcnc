<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue';
import { ClassicladderClient, type Variables } from '../generated/classicladder_client';

const client = new ClassicladderClient(window.location.origin);
const vars = ref<Variables | null>(null);
const pollTimer = ref<ReturnType<typeof setInterval> | null>(null);
const filter = ref('');

// Variable categories to display
const categories = [
  { key: 'bits', label: '%B (Memory Bits)', prefix: '%B' },
  { key: 'physInputs', label: '%I (Inputs)', prefix: '%I' },
  { key: 'physOutputs', label: '%Q (Outputs)', prefix: '%Q' },
  { key: 'errorBits', label: '%E (Errors)', prefix: '%E' },
  { key: 'words', label: '%W (Words)', prefix: '%W' },
] as const;

const activeCategory = ref<string>('bits');

async function refresh() {
  try {
    vars.value = await client.getVariables();
  } catch {
    // ignore poll errors
  }
}

function getBoolArray(): { index: number; value: boolean }[] {
  if (!vars.value) return [];
  let arr: boolean[] = [];
  switch (activeCategory.value) {
    case 'bits': arr = vars.value.bools.bits; break;
    case 'physInputs': arr = vars.value.bools.physInputs; break;
    case 'physOutputs': arr = vars.value.bools.physOutputs; break;
    case 'errorBits': arr = vars.value.bools.errorBits; break;
    default: return [];
  }
  return arr.map((v, i) => ({ index: i, value: v }))
    .filter(item => {
      if (!filter.value) return true;
      const cat = categories.find(c => c.key === activeCategory.value);
      return `${cat?.prefix}${item.index}`.toLowerCase().includes(filter.value.toLowerCase());
    });
}

function getWordArray(): { index: number; value: number }[] {
  if (!vars.value || activeCategory.value !== 'words') return [];
  return vars.value.words.words.map((v, i) => ({ index: i, value: v }))
    .filter(item => {
      if (!filter.value) return true;
      return `%W${item.index}`.toLowerCase().includes(filter.value.toLowerCase());
    });
}

onMounted(() => {
  refresh();
  pollTimer.value = setInterval(refresh, 500);
});

onUnmounted(() => {
  if (pollTimer.value) clearInterval(pollTimer.value);
});
</script>

<template>
  <div class="var-spy">
    <div class="spy-toolbar">
      <select v-model="activeCategory">
        <option v-for="cat in categories" :key="cat.key" :value="cat.key">
          {{ cat.label }}
        </option>
      </select>
      <input v-model="filter" placeholder="Filter..." class="filter-input" />
    </div>

    <div class="var-grid" v-if="activeCategory !== 'words'">
      <div
        v-for="item in getBoolArray()"
        :key="item.index"
        class="var-bit"
        :class="{ on: item.value }"
      >
        <span class="var-name">{{ categories.find(c => c.key === activeCategory)?.prefix }}{{ item.index }}</span>
        <span class="var-val">{{ item.value ? '1' : '0' }}</span>
      </div>
    </div>

    <div class="var-grid words" v-else>
      <div v-for="item in getWordArray()" :key="item.index" class="var-word">
        <span class="var-name">%W{{ item.index }}</span>
        <span class="var-val">{{ item.value }}</span>
      </div>
    </div>
  </div>
</template>

<style scoped>
.var-spy {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.spy-toolbar {
  display: flex;
  gap: 8px;
  margin-bottom: 12px;
}

.spy-toolbar select {
  width: 200px;
}

.filter-input {
  flex: 1;
}

.var-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(80px, 1fr));
  gap: 4px;
  overflow-y: auto;
  flex: 1;
}

.var-grid.words {
  grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
}

.var-bit, .var-word {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 3px 6px;
  background: #1e1e2e;
  border-radius: 3px;
  font-size: 11px;
  border: 1px solid #45475a;
}

.var-bit.on {
  background: #1e3a2e;
  border-color: #a6e3a1;
}

.var-name {
  color: #89b4fa;
  font-family: monospace;
  font-size: 10px;
}

.var-val {
  font-weight: 700;
  font-family: monospace;
}

.var-bit.on .var-val {
  color: #a6e3a1;
}
</style>
