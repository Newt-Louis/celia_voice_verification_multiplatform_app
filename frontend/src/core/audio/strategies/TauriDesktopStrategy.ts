import { invoke } from '@tauri-apps/api/core'
import type { AudioInputLevel, AudioStartResult, AudioStopResult, AudioStrategy } from '../types'

type NativeAudioResponse = {
  request_id: string | null
  message: string
}

type NativeAudioLevelResponse = {
  rms: number
  peak: number
  sample_rate: number
  channels: number
  device_name: string
  status: 'idle' | 'recording'
  updated_at_ms: number
}

export function createTauriDesktopStrategy(): AudioStrategy {
  return {
    target: 'tauri-desktop',
    async startRecording(): Promise<AudioStartResult> {
      const response = await invoke<NativeAudioResponse>('audio_start_recording')

      return {
        requestId: response.request_id ?? crypto.randomUUID(),
        runtimeTarget: 'tauri-desktop',
        message: response.message
      }
    },
    async stopRecording(requestId: string | null): Promise<AudioStopResult> {
      const response = await invoke<NativeAudioResponse>('audio_stop_recording', {
        requestId
      })

      return {
        requestId: response.request_id,
        runtimeTarget: 'tauri-desktop',
        message: response.message
      }
    },
    async getInputLevel(): Promise<AudioInputLevel> {
      const response = await invoke<NativeAudioLevelResponse>('audio_get_input_level')

      return {
        rms: response.rms,
        peak: response.peak,
        sampleRate: response.sample_rate,
        channels: response.channels,
        deviceName: response.device_name,
        status: response.status,
        updatedAtMs: response.updated_at_ms
      }
    }
  }
}
