import type { AudioInputLevel, AudioStartResult, AudioStopResult, AudioStrategy, RuntimeTarget } from '../types'

type NativeStartResponse = {
  requestId?: string | null
  message?: string
}

type NativeStopResponse = {
  requestId?: string | null
  message?: string
}

type NativeInputLevelResponse = {
  rms?: number
  peak?: number
  processedRms?: number
  processedPeak?: number
  noiseFloor?: number
  vadProbability?: number
  sampleRate?: number
  channels?: number
  deviceName?: string
  status?: 'idle' | 'recording'
  vadActive?: boolean
  speechFrames?: number
  updatedAtMs?: number
  transcriptionStatus?: AudioInputLevel['transcriptionStatus']
  transcript?: string
}

type NativeFunction = (...args: unknown[]) => Promise<unknown>

declare global {
  interface Window {
    celiaAudioStartRecording?: NativeFunction
    celiaAudioStopRecording?: NativeFunction
    celiaAudioGetInputLevel?: NativeFunction
  }
}

function ensureNativeFunction(name: 'celiaAudioStartRecording' | 'celiaAudioStopRecording' | 'celiaAudioGetInputLevel'): NativeFunction {
  const fn = window[name]
  if (typeof fn !== 'function') {
    throw new Error('Native C++ bridge chưa sẵn sàng.')
  }
  return fn
}

function asObject<T>(value: unknown): T {
  if (typeof value === 'string') {
    return JSON.parse(value) as T
  }
  return value as T
}

export function createNativeBridgeStrategy(target: Exclude<RuntimeTarget, 'web'>): AudioStrategy {
  return {
    target,
    async startRecording(): Promise<AudioStartResult> {
      const response = asObject<NativeStartResponse>(await ensureNativeFunction('celiaAudioStartRecording')())

      return {
        requestId: response.requestId ?? crypto.randomUUID(),
        runtimeTarget: target,
        message: response.message ?? 'C++ native audio pipeline đã khởi động.'
      }
    },
    async stopRecording(requestId: string | null): Promise<AudioStopResult> {
      const response = asObject<NativeStopResponse>(await ensureNativeFunction('celiaAudioStopRecording')(requestId))

      return {
        requestId: response.requestId ?? requestId,
        runtimeTarget: target,
        message: response.message ?? 'C++ native audio pipeline đã dừng.'
      }
    },
    async getInputLevel(): Promise<AudioInputLevel> {
      const response = asObject<NativeInputLevelResponse>(await ensureNativeFunction('celiaAudioGetInputLevel')())

      return {
        rms: response.rms ?? 0,
        peak: response.peak ?? 0,
        processedRms: response.processedRms ?? 0,
        processedPeak: response.processedPeak ?? 0,
        noiseFloor: response.noiseFloor ?? 0,
        vadProbability: response.vadProbability ?? 0,
        sampleRate: response.sampleRate ?? 0,
        channels: response.channels ?? 0,
        deviceName: response.deviceName ?? 'No active input',
        status: response.status ?? 'idle',
        vadActive: response.vadActive ?? false,
        speechFrames: response.speechFrames ?? 0,
        updatedAtMs: response.updatedAtMs ?? 0,
        transcriptionStatus: response.transcriptionStatus ?? 'idle',
        transcript: response.transcript ?? ''
      }
    }
  }
}
