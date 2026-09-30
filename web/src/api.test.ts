import { describe, expect, it } from 'vitest'
import { resolveApiBase } from './api'

describe('API base URL', () => {
  it('uses the development proxy only in development builds', () => {
    expect(resolveApiBase(undefined, true)).toBe('/api')
    expect(resolveApiBase(undefined, false)).toBe('')
  })

  it('honors an explicit deployment URL without a trailing slash', () => {
    expect(resolveApiBase('https://solver.example/api/', false)).toBe('https://solver.example/api')
  })
})
