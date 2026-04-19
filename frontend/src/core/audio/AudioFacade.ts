import type { AudioInputLevel, AudioStartResult, AudioStopResult, AudioStrategy, RuntimeTarget } from './types'
import { createAutomotiveStrategy } from './strategies/AutomotiveStrategy'
import { createTauriDesktopStrategy } from './strategies/TauriDesktopStrategy'
import { createTauriMobileStrategy } from './strategies/TauriMobileStrategy'
import { createWebStrategy } from './strategies/WebStrategy'

const SUPPORTED_TARGETS: RuntimeTarget[] = ['web', 'tauri-desktop', 'tauri-mobile', 'automotive']

function resolveRuntimeTarget(): RuntimeTarget {
  const configuredTarget = import.meta.env.VITE_CELIA_RUNTIME_TARGET
  if (configuredTarget && SUPPORTED_TARGETS.includes(configuredTarget)) {
    return configuredTarget
  }

  if (typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window) {
    return 'tauri-desktop'
  }

  return 'web'
}

function createStrategy(target: RuntimeTarget): AudioStrategy {
  switch (target) {
    case 'tauri-desktop':
      return createTauriDesktopStrategy()
    case 'tauri-mobile':
      return createTauriMobileStrategy()
    case 'automotive':
      return createAutomotiveStrategy()
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
