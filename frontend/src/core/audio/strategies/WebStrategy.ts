import type { AudioInputLevel, AudioStartResult, AudioStopResult, AudioStrategy } from '../types'

export function createWebStrategy(): AudioStrategy {
  let stream: MediaStream | null = null
  let audioContext: AudioContext | null = null
  let analyser: AnalyserNode | null = null
  let timeDomainData: Float32Array<ArrayBuffer> | null = null

  return {
    target: 'web',
    async startRecording(): Promise<AudioStartResult> {
      stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: true,
          noiseSuppression: true,
          autoGainControl: false
        }
      })
      audioContext = new AudioContext()
      analyser = audioContext.createAnalyser()
      analyser.fftSize = 1024
      timeDomainData = new Float32Array(new ArrayBuffer(analyser.fftSize * Float32Array.BYTES_PER_ELEMENT))
      audioContext.createMediaStreamSource(stream).connect(analyser)

      return {
        requestId: crypto.randomUUID(),
        runtimeTarget: 'web',
        message: 'Web Audio API đã nhận luồng micro.'
      }
    },
    async stopRecording(requestId: string | null): Promise<AudioStopResult> {
      await audioContext?.close()
      audioContext = null
      analyser = null
      timeDomainData = null
      stream?.getTracks().forEach((track) => track.stop())
      stream = null

      return {
        requestId,
        runtimeTarget: 'web',
        message: 'Đã dừng luồng micro trên trình duyệt.'
      }
    },
    async getInputLevel(): Promise<AudioInputLevel> {
      if (!stream || !audioContext || !analyser || !timeDomainData) {
        return {
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
          transcript: ''
        }
      }

      analyser.getFloatTimeDomainData(timeDomainData)

      let sum = 0
      let peak = 0
      for (const sample of timeDomainData) {
        const value = Math.max(-1, Math.min(1, sample))
        const abs = Math.abs(value)
        peak = Math.max(peak, abs)
        sum += value * value
      }

      const track = stream.getAudioTracks()[0]
      const settings = track?.getSettings()

      return {
        rms: Math.sqrt(sum / timeDomainData.length),
        peak,
        processedRms: Math.sqrt(sum / timeDomainData.length),
        processedPeak: peak,
        noiseFloor: 0,
        vadProbability: peak > 0.03 ? 1 : 0,
        sampleRate: audioContext.sampleRate,
        channels: settings?.channelCount ?? 1,
        deviceName: track?.label || 'Browser microphone',
        status: 'recording',
        vadActive: peak > 0.03,
        speechFrames: 0,
        updatedAtMs: Date.now(),
        transcriptionStatus: 'waiting_for_whisper_cpp',
        transcript: ''
      }
    }
  }
}
