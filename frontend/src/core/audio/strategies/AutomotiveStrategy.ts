import { invoke } from '@tauri-apps/api/core'
import type { AudioInputLevel, AudioStartResult, AudioStopResult, AudioStrategy } from '../types'

type NativeAudioResponse = {
  request_id: string | null
  message: string
}

export function createAutomotiveStrategy(): AudioStrategy {
  return {
    target: 'automotive',
    async startRecording(): Promise<AudioStartResult> {
      const response = await invoke<NativeAudioResponse>('audio_start_recording')

      return {
        requestId: response.request_id ?? crypto.randomUUID(),
        runtimeTarget: 'automotive',
        message: response.message
      }
    },
    async stopRecording(requestId: string | null): Promise<AudioStopResult> {
      const response = await invoke<NativeAudioResponse>('audio_stop_recording', {
        requestId
      })

      return {
        requestId: response.request_id,
        runtimeTarget: 'automotive',
        message: response.message
      }
    },
    async getInputLevel(): Promise<AudioInputLevel> {
      return {
        rms: 0,
        peak: 0,
        sampleRate: 0,
        channels: 0,
        deviceName: 'Automotive input monitor pending',
        status: 'idle',
        updatedAtMs: 0
      }
    }
  }
}
