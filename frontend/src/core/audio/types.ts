export type RuntimeTarget = 'web' | 'native-desktop' | 'native-mobile' | 'automotive'

export type AudioStatus = 'idle' | 'starting' | 'recording' | 'stopping' | 'error'

export type AudioStartResult = {
  requestId: string
  runtimeTarget: RuntimeTarget
  message: string
}

export type AudioStopResult = {
  requestId: string | null
  runtimeTarget: RuntimeTarget
  message: string
}

export type AudioInputLevel = {
  rms: number
  peak: number
  processedRms: number
  processedPeak: number
  noiseFloor: number
  vadProbability: number
  sampleRate: number
  channels: number
  deviceName: string
  status: 'idle' | 'recording'
  vadActive: boolean
  speechFrames: number
  updatedAtMs: number
  transcriptionStatus: string
  transcript: string
}

export interface AudioStrategy {
  readonly target: RuntimeTarget
  startRecording(): Promise<AudioStartResult>
  stopRecording(requestId: string | null): Promise<AudioStopResult>
  getInputLevel(): Promise<AudioInputLevel>
}
