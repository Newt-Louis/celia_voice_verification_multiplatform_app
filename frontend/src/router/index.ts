import { createRouter, createWebHistory } from 'vue-router'
import VoiceConsoleView from '@/views/VoiceConsoleView.vue'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      name: 'voice-console',
      component: VoiceConsoleView
    }
  ]
})

export default router
