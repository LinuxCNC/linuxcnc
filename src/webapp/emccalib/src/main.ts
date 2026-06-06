import { createApp } from 'vue';
import App from './App.vue';
import { calibStore } from './stores/calib';

createApp(App).mount('#app');

// Auto-load tunables on startup
calibStore.loadTunables();
