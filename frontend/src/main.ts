import { createApp } from 'vue'
import { createPinia } from 'pinia'
import Aura from '@primeuix/themes/aura'
import PrimeVue from 'primevue/config'
import Button from 'primevue/button'
import Tag from 'primevue/tag'
import ToastService from 'primevue/toastservice'
import 'primeicons/primeicons.css'
import './styles.css'

import App from './App.vue'
import router from './router'

const app = createApp(App)

app.use(createPinia())
app.use(router)
app.use(PrimeVue, {
  ripple: true,
  theme: {
    preset: Aura
  }
})
app.use(ToastService)

app.component('Button', Button)
app.component('Tag', Tag)

app.mount('#app')
