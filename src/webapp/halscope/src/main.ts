import { createApp } from 'vue';
import App from './App.vue';
import { scopeStore } from './stores/scope';

createApp(App).mount('#app');

// Auto-connect on load
scopeStore.connect();
