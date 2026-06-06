import { createApp } from 'vue';
import App from './App.vue';
import { halshowStore } from './stores/halshow';

createApp(App).mount('#app');

// Auto-connect on load
halshowStore.connect();
