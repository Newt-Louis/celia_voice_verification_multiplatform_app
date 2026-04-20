import { createRouter, createWebHashHistory } from 'vue-router'
import VoiceConsoleView from '@/views/VoiceConsoleView.vue'

const router = createRouter({
  history: createWebHashHistory(),
  routes: [
    {
      path: '/',
      name: 'voice-console',
      component: VoiceConsoleView
    }
  ]
})

export default router
