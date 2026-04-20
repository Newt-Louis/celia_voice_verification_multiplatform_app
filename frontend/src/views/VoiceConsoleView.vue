<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue'
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
const processedPeakPercent = computed(() => Math.min(100, Math.round(session.inputLevel.processedPeak * 100)))
const vadPercent = computed(() => Math.min(100, Math.round(session.inputLevel.vadProbability * 100)))
const transcriptDraft = ref('')
let lastNativeTranscript = ''
const inputConfig = computed(() => {
  if (session.inputLevel.status === 'idle') return 'Chưa có stream micro'
  return `${session.inputLevel.sampleRate} Hz · ${session.inputLevel.channels} kênh`
})
const vadLabel = computed(() => (session.inputLevel.vadActive ? 'Có giọng nói' : 'Đang lọc nền'))

watch(
  () => session.inputLevel.transcript,
  (transcript) => {
    if (!transcript || transcript === lastNativeTranscript) return
    lastNativeTranscript = transcript
    transcriptDraft.value = transcript
  },
  { immediate: true }
)

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
          <div class="level-meter level-meter--processed" aria-label="Mức tín hiệu sau lọc">
            <span :style="{ width: `${processedPeakPercent}%` }"></span>
          </div>
          <div class="level-numbers">
            <span>NS {{ processedPeakPercent }}%</span>
            <span>VAD {{ vadPercent }}%</span>
          </div>
          <p class="vad-state" :class="{ 'vad-state--active': session.inputLevel.vadActive }">
            {{ vadLabel }}
          </p>
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

      <section class="transcript-console" aria-label="Realtime transcript">
        <div class="transcript-console__header">
          <p class="label">Realtime transcript</p>
          <strong>{{ session.inputLevel.transcriptionStatus }}</strong>
        </div>
        <textarea
          v-model="transcriptDraft"
          rows="15"
          placeholder="Đang chờ giọng nói sau VAD để Whisper sinh văn bản..."
          aria-label="Văn bản sinh ra từ Whisper"
        ></textarea>
      </section>

      <section class="pipeline-grid" aria-label="Các lớp xử lý">
        <article>
          <span>01</span>
          <h2>UI</h2>
          <p>PrimeVue chỉ điều khiển hiển thị và gọi facade.</p>
        </article>
        <article>
          <span>02</span>
          <h2>Facade</h2>
          <p>Chọn strategy theo Vite ENV hoặc bridge C++ native.</p>
        </article>
        <article>
          <span>03</span>
          <h2>Strategy</h2>
          <p>Web và native targets đi qua facade chung.</p>
        </article>
        <article>
          <span>04</span>
          <h2>Native</h2>
          <p>C++ mở micro bằng miniaudio và điều phối webview.</p>
        </article>
      </section>
    </section>
  </main>
</template>
