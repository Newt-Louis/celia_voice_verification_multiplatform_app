<script setup lang="ts">
import { computed, onBeforeUnmount } from 'vue'
import { audioFacade } from '@/core/audio/AudioFacade'
import { useAudioSessionStore } from '@/stores/audioSession'

const session = useAudioSessionStore()
session.setRuntimeTarget(audioFacade.target)
let levelPoller: number | null = null

const isRecording = computed(() => session.status === 'recording' || session.status === 'starting')
const statusSeverity = computed(() => {
  if (session.status === 'recording') return 'success'
  if (session.status === 'error') return 'danger'
  if (session.status === 'starting' || session.status === 'stopping') return 'warning'
  return 'secondary'
})
const peakPercent = computed(() => Math.min(100, Math.round(session.inputLevel.peak * 100)))
const rmsPercent = computed(() => Math.min(100, Math.round(session.inputLevel.rms * 100)))
const inputConfig = computed(() => {
  if (session.inputLevel.status === 'idle') return 'Chưa có stream micro'
  return `${session.inputLevel.sampleRate} Hz · ${session.inputLevel.channels} kênh`
})

function stopLevelPolling() {
  if (levelPoller !== null) {
    window.clearInterval(levelPoller)
    levelPoller = null
  }
}

async function refreshInputLevel() {
  if (!isRecording.value) return

  try {
    const level = await audioFacade.getInputLevel()
    session.setInputLevel(level)
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Không đọc được mức tín hiệu micro.'
    session.setStatus('error', message)
    stopLevelPolling()
  }
}

function startLevelPolling() {
  stopLevelPolling()
  void refreshInputLevel()
  levelPoller = window.setInterval(() => {
    void refreshInputLevel()
  }, 120)
}

async function toggleRecording() {
  try {
    if (isRecording.value) {
      session.setStatus('stopping', 'Đang dừng phiên thu âm.')
      stopLevelPolling()
      const result = await audioFacade.stopRecording(session.activeRequestId)
      session.setActiveRequest(null)
      session.resetInputLevel()
      session.setStatus('idle', result.message)
      return
    }

    session.setStatus('starting', 'Đang mở phiên thu âm.')
    const result = await audioFacade.startRecording()
    session.setActiveRequest(result.requestId)
    session.setStatus('recording', result.message)
    startLevelPolling()
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Không thể khởi động audio pipeline.'
    session.setStatus('error', message)
    stopLevelPolling()
  }
}

onBeforeUnmount(() => {
  stopLevelPolling()
})
</script>

<template>
  <main class="app-shell">
    <section class="workspace">
      <div class="brand-row">
        <div>
          <p class="eyebrow">Celia Voice Verification</p>
          <h1>Kiểm tra pipeline giọng nói</h1>
        </div>
        <Tag :value="session.runtimeTarget" :severity="statusSeverity" />
      </div>

      <div class="control-surface">
        <div class="mic-state" :class="{ 'mic-state--active': isRecording }">
          <i class="pi pi-microphone"></i>
        </div>

        <div class="session-copy">
          <p class="label">Trạng thái</p>
          <strong>{{ session.status }}</strong>
          <p>{{ session.lastMessage }}</p>
        </div>

        <div class="level-panel">
          <p class="label">Tín hiệu micro</p>
          <div class="level-meter" aria-label="Mức peak micro">
            <span :style="{ width: `${peakPercent}%` }"></span>
          </div>
          <div class="level-numbers">
            <span>Peak {{ peakPercent }}%</span>
            <span>RMS {{ rmsPercent }}%</span>
          </div>
          <p>{{ inputConfig }}</p>
          <small>{{ session.inputLevel.deviceName }}</small>
        </div>

        <Button
          class="record-button"
          :severity="isRecording ? 'danger' : 'success'"
          :label="isRecording ? 'Dừng thu' : 'Bắt đầu thu'"
          :icon="isRecording ? 'pi pi-stop' : 'pi pi-play'"
          :loading="session.status === 'starting' || session.status === 'stopping'"
          @click="toggleRecording"
        />
      </div>

      <section class="pipeline-grid" aria-label="Các lớp xử lý">
        <article>
          <span>01</span>
          <h2>UI</h2>
          <p>PrimeVue chỉ điều khiển hiển thị và gọi facade.</p>
        </article>
        <article>
          <span>02</span>
          <h2>Facade</h2>
          <p>Chọn strategy theo Vite ENV hoặc môi trường Tauri.</p>
        </article>
        <article>
          <span>03</span>
          <h2>Strategy</h2>
          <p>Web, Desktop, Mobile và Automotive tách adapter riêng.</p>
        </article>
        <article>
          <span>04</span>
          <h2>Native</h2>
          <p>Rust đo tín hiệu micro, C++ để dành cho xử lý audio/model nặng.</p>
        </article>
      </section>
    </section>
  </main>
</template>
