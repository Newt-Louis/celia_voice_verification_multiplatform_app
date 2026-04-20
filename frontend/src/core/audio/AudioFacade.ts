import type { AudioInputLevel, AudioStartResult, AudioStopResult, AudioStrategy, RuntimeTarget } from './types'
import { createNativeBridgeStrategy } from './strategies/NativeBridgeStrategy'
import { createWebStrategy } from './strategies/WebStrategy'

const SUPPORTED_TARGETS: RuntimeTarget[] = ['web', 'native-desktop', 'native-mobile', 'automotive']

function hasNativeBridge(): boolean {
  return typeof window !== 'undefined' && typeof window.celiaAudioStartRecording === 'function'
}

function resolveRuntimeTarget(): RuntimeTarget {
  const configuredTarget = import.meta.env.VITE_CELIA_RUNTIME_TARGET
  if (configuredTarget && SUPPORTED_TARGETS.includes(configuredTarget)) {
    return configuredTarget
  }

  if (hasNativeBridge()) {
    return 'native-desktop'
  }

  return 'web'
}

function createStrategy(target: RuntimeTarget): AudioStrategy {
  switch (target) {
    case 'native-desktop':
    case 'native-mobile':
    case 'automotive':
      return createNativeBridgeStrategy(target)
    case 'web':
    default:
      return createWebStrategy()
  }
}

class AudioFacade {
  private readonly strategy: AudioStrategy

  constructor() {
    this.strategy = createStrategy(resolveRuntimeTarget())
  }

  get target(): RuntimeTarget {
    return this.strategy.target
  }

  startRecording(): Promise<AudioStartResult> {
    return this.strategy.startRecording()
  }

  stopRecording(requestId: string | null): Promise<AudioStopResult> {
    return this.strategy.stopRecording(requestId)
  }

  getInputLevel(): Promise<AudioInputLevel> {
    return this.strategy.getInputLevel()
  }
}

export const audioFacade = new AudioFacade()
