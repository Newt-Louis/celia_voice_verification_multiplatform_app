import { defineStore } from 'pinia'
import type { AudioInputLevel, AudioStatus, RuntimeTarget } from '@/core/audio/types'

type SessionState = {
  status: AudioStatus
  runtimeTarget: RuntimeTarget
  lastMessage: string
  activeRequestId: string | null
  inputLevel: AudioInputLevel
}

const emptyInputLevel = (): AudioInputLevel => ({
  rms: 0,
  peak: 0,
  processedRms: 0,
  processedPeak: 0,
  noiseFloor: 0,
  vadProbability: 0,
  sampleRate: 0,
  channels: 0,
  deviceName: 'No active input',
  status: 'idle',
  vadActive: false,
  speechFrames: 0,
  updatedAtMs: 0,
  transcriptionStatus: 'idle',
  transcript: '',
  processingMode: 'custom-dsp',
  processingDetails: 'Custom DSP NS/VAD'
})

export const useAudioSessionStore = defineStore('audioSession', {
  state: (): SessionState => ({
    status: 'idle',
    runtimeTarget: 'web',
    lastMessage: 'Sẵn sàng kiểm tra giọng nói.',
    activeRequestId: null,
    inputLevel: emptyInputLevel()
  }),
  actions: {
    setRuntimeTarget(target: RuntimeTarget) {
      this.runtimeTarget = target
    },
    setStatus(status: AudioStatus, message: string) {
      this.status = status
      this.lastMessage = message
    },
    setActiveRequest(requestId: string | null) {
      this.activeRequestId = requestId
    },
    setInputLevel(level: AudioInputLevel) {
      this.inputLevel = level
    },
    resetInputLevel() {
      this.inputLevel = emptyInputLevel()
    }
  }
})
