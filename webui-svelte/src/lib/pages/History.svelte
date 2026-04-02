<script lang="ts">
  import { onMount } from 'svelte'
  import PageLayout from '../components/PageLayout.svelte'
  import { telemetry } from '../stores/telemetry'
  import {
    getHistoryEvents,
    getHistorySessions,
    getStatisticsSummary,
    type HistoryEventItem,
    type HistorySessionItem,
    type HistorySummaryResponse,
  } from '../api/rest'

  let sidebarCollapsed = false
  let loading = true
  let refreshing = false
  let error = ''

  let summary: HistorySummaryResponse | null = null
  let events: HistoryEventItem[] = []
  let sessions: HistorySessionItem[] = []

  function formatDate(tsMs?: number) {
    if (!tsMs) return '—'
    return new Date(tsMs).toLocaleString('de-DE', {
      dateStyle: 'short',
      timeStyle: 'medium',
    })
  }

  function formatUptime(tsMs?: number) {
    if (!tsMs) return '—'
    const totalSeconds = Math.max(0, Math.round(tsMs / 1000))
    const hours = Math.floor(totalSeconds / 3600)
    const minutes = Math.floor((totalSeconds % 3600) / 60)
    const seconds = totalSeconds % 60

    if (hours > 0) return `${hours} h ${minutes} min ${seconds}s`
    if (minutes > 0) return `${minutes} min ${seconds}s`
    return `${seconds}s`
  }

  function formatDuration(durationMs?: number) {
    if (!durationMs) return '0 min'
    const totalSeconds = Math.max(0, Math.round(durationMs / 1000))
    const hours = Math.floor(totalSeconds / 3600)
    const minutes = Math.floor((totalSeconds % 3600) / 60)
    const seconds = totalSeconds % 60
    if (hours > 0) return `${hours} h ${minutes} min`
    if (minutes > 0) return `${minutes} min ${seconds}s`
    return `${seconds}s`
  }

  function formatMeters(distance?: number) {
    if (distance == null) return '—'
    return `${distance.toFixed(1)} m`
  }

  function formatVoltage(voltage?: number) {
    if (voltage == null) return '—'
    return `${voltage.toFixed(1)} V`
  }

  function toneForLevel(level: string) {
    if (level === 'error') return 'error'
    if (level === 'warn') return 'warn'
    return 'info'
  }

  function humanize(value: string) {
    if (!value) return '—'
    return value
      .replace(/^ERR_/, '')
      .replace(/_/g, ' ')
      .toLowerCase()
      .replace(/^\w/, (char) => char.toUpperCase())
  }

  function topCounts(source: Record<string, number> | undefined, limit = 8) {
    if (!source) return []
    return Object.entries(source)
      .sort((a, b) => b[1] - a[1])
      .slice(0, limit)
  }

  async function refreshHistory() {
    refreshing = true
    error = ''
    try {
      const [summaryResult, eventsResult, sessionsResult] = await Promise.all([
        getStatisticsSummary(),
        getHistoryEvents(40),
        getHistorySessions(20),
      ])

      summary = summaryResult
      events = eventsResult.items ?? []
      sessions = sessionsResult.items ?? []
    } catch (err) {
      error = err instanceof Error ? err.message : 'Verlauf konnte nicht geladen werden.'
    } finally {
      loading = false
      refreshing = false
    }
  }

  onMount(() => {
    void refreshHistory()
  })

  $: topReasons = topCounts(summary?.event_reason_counts)
  $: topTypes = topCounts(summary?.event_type_counts, 5)
  $: topLevels = topCounts(summary?.event_level_counts, 5)
  $: backendReady = summary?.backend_ready ?? false
</script>

<PageLayout
  {sidebarCollapsed}
  on:toggle={() => (sidebarCollapsed = !sidebarCollapsed)}
>
  <div class="history-page">
    <div class="hero">
      <div>
        <span class="eyebrow">Verlauf & Statistik</span>
        <h1>Feldereignisse, Sitzungen und Runtime-Trends</h1>
        <p>
          Diese Ansicht nutzt die persistierte History-DB und zeigt die wichtigsten
          Vorfälle direkt als Zähler, Sessions und letzte Ereignisse.
        </p>
      </div>

      <div class="hero-actions">
        <button type="button" class="refresh-btn" on:click={() => void refreshHistory()} disabled={refreshing}>
          {refreshing ? 'Aktualisiere…' : 'Aktualisieren'}
        </button>
        {#if summary?.export_enabled && summary?.database_exists}
          <a class="export-btn" href="/api/history/export">DB exportieren</a>
        {/if}
      </div>
    </div>

    {#if error}
      <div class="notice error">{error}</div>
    {/if}

    {#if loading}
      <div class="notice">Verlauf wird geladen…</div>
    {:else}
      <section class="stats-grid">
        <article class="stat-card">
          <span class="label">Backend</span>
          <strong>{backendReady ? 'bereit' : 'nicht bereit'}</strong>
          <span class="muted">
            {$telemetry.history_backend_ready ? 'Live-Telemetrie sieht Backend' : 'Telemetrie meldet kein Backend'}
          </span>
        </article>

        <article class="stat-card">
          <span class="label">Events gesamt</span>
          <strong>{summary?.events_total ?? 0}</strong>
          <span class="muted">Letztes Event {formatDate(summary?.last_event_wall_ts_ms)}</span>
        </article>

        <article class="stat-card">
          <span class="label">Sessions</span>
          <strong>{summary?.sessions_completed ?? 0} / {summary?.sessions_total ?? 0}</strong>
          <span class="muted">Abgeschlossen / insgesamt</span>
        </article>

        <article class="stat-card">
          <span class="label">Mähzeit gesamt</span>
          <strong>{formatDuration(summary?.mowing_duration_ms_total)}</strong>
          <span class="muted">Strecke {formatMeters(summary?.mowing_distance_m_total)}</span>
        </article>
      </section>

      <section class="content-grid">
        <div class="panel wide">
          <div class="panel-header">
            <div>
              <span class="label">Top-Vorfallgründe</span>
              <h2>Häufigste Event-Gründe</h2>
            </div>
          </div>

          {#if topReasons.length === 0}
            <div class="empty">Noch keine gruppierten Ereignisgründe vorhanden.</div>
          {:else}
            <div class="count-grid">
              {#each topReasons as [reason, count]}
                <div class="count-card">
                  <strong>{count}</strong>
                  <span>{humanize(reason)}</span>
                </div>
              {/each}
            </div>
          {/if}
        </div>

        <div class="panel">
          <div class="panel-header">
            <div>
              <span class="label">Event-Arten</span>
              <h2>Typen</h2>
            </div>
          </div>
          <div class="mini-list">
            {#each topTypes as [type, count]}
              <div class="mini-row"><span>{humanize(type)}</span><strong>{count}</strong></div>
            {/each}
          </div>
        </div>

        <div class="panel">
          <div class="panel-header">
            <div>
              <span class="label">Severity</span>
              <h2>Level</h2>
            </div>
          </div>
          <div class="mini-list">
            {#each topLevels as [level, count]}
              <div class="mini-row"><span>{humanize(level)}</span><strong>{count}</strong></div>
            {/each}
          </div>
        </div>

        <div class="panel wide">
          <div class="panel-header">
            <div>
              <span class="label">Letzte Fahrten</span>
              <h2>Sitzungen</h2>
            </div>
          </div>

          {#if sessions.length === 0}
            <div class="empty">Noch keine Sessions gespeichert.</div>
          {:else}
            <div class="table-wrap">
              <table>
                <thead>
                  <tr>
                    <th>Start</th>
                    <th>Dauer</th>
                    <th>Strecke</th>
                    <th>Batterie</th>
                    <th>Ende</th>
                  </tr>
                </thead>
                <tbody>
                  {#each sessions as session}
                    <tr>
                      <td>{formatDate(session.started_at_ms)}</td>
                      <td>{formatDuration(session.duration_ms)}</td>
                      <td>{formatMeters(session.distance_m)}</td>
                      <td>{formatVoltage(session.battery_start_v)} → {formatVoltage(session.battery_end_v)}</td>
                      <td>{humanize(session.end_reason)}</td>
                    </tr>
                  {/each}
                </tbody>
              </table>
            </div>
          {/if}
        </div>

        <div class="panel wide">
          <div class="panel-header">
            <div>
              <span class="label">Letzte Ereignisse</span>
              <h2>Event-Stream</h2>
            </div>
          </div>

          {#if events.length === 0}
            <div class="empty">Noch keine Events gespeichert.</div>
          {:else}
            <div class="events">
              {#each events as event}
                <article class={`event-card ${toneForLevel(event.level)}`}>
                  <div class="event-top">
                    <span class="event-time">{formatDate(event.wall_ts_ms)}</span>
                    <span class="event-level">{humanize(event.level)}</span>
                  </div>
                  <strong>{event.message || humanize(event.event_reason)}</strong>
                  <div class="event-meta">
                    <span>{humanize(event.event_type)}</span>
                    <span>{humanize(event.event_reason)}</span>
                    <span>{event.module || 'Robot'}</span>
                    {#if event.error_code}
                      <span>{event.error_code}</span>
                    {/if}
                  </div>
                </article>
              {/each}
            </div>
          {/if}
        </div>
      </section>
    {/if}
  </div>

  <svelte:fragment slot="sidebar">
    <div class="sidebar-header">
      <span class="eyebrow">History DB</span>
      <h2>Live-Kontext</h2>
      <p>Persistente Statistik plus aktueller Runtime-Zustand für schnelle Felddiagnose.</p>
    </div>

    <div class={`state-card ${backendReady ? 'success' : 'warning'}`}>
      <span class="label">Verfügbarkeit</span>
      <strong>{backendReady ? 'History aktiv' : 'History nicht bereit'}</strong>
      <span>{summary?.database_path || 'Pfad unbekannt'}</span>
    </div>

    <div class="state-card info">
      <span class="label">Live-Runtime</span>
      <strong>{$telemetry.runtime_health || 'ok'}</strong>
      <span>Op {$telemetry.op} · Grund {humanize($telemetry.event_reason)}</span>
      <span>Fehler {$telemetry.error_code || '—'}</span>
    </div>

    <div class="state-card">
      <span class="label">Retention</span>
      <strong>{summary?.retention.max_events ?? 0} Events</strong>
      <span>{summary?.retention.max_sessions ?? 0} Sessions</span>
    </div>

    <div class="state-card">
      <span class="label">Letzter Verlaufseintrag</span>
      <strong>{formatDate(summary?.last_event_wall_ts_ms)}</strong>
      <span>Letzter Session-Start {formatDate(summary?.last_session_started_at_ms)}</span>
    </div>
  </svelte:fragment>
</PageLayout>

<style>
  .history-page {
    height: 100%;
    overflow-y: auto;
    padding: 1rem;
    display: grid;
    gap: 1rem;
    background:
      radial-gradient(circle at top left, rgba(14, 116, 144, 0.16), transparent 32%),
      linear-gradient(180deg, rgba(7, 13, 24, 0.98), rgba(10, 15, 26, 0.98));
  }

  .hero {
    display: flex;
    justify-content: space-between;
    gap: 1rem;
    align-items: end;
    padding: 1rem 1.1rem;
    border-radius: 1rem;
    background: rgba(15, 24, 41, 0.95);
    border: 1px solid #1e3a5f;
  }

  .eyebrow {
    display: inline-block;
    color: #67e8f9;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    font-size: 0.72rem;
    margin-bottom: 0.35rem;
  }

  h1,
  h2 {
    margin: 0;
  }

  .hero p,
  .sidebar-header p {
    margin: 0.45rem 0 0;
    color: #94a3b8;
    line-height: 1.45;
  }

  .hero-actions {
    display: flex;
    gap: 0.6rem;
    align-items: center;
    flex-wrap: wrap;
  }

  .refresh-btn,
  .export-btn {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    padding: 0.7rem 0.95rem;
    border-radius: 0.65rem;
    border: 1px solid #1e3a5f;
    background: #10203a;
    color: #dbeafe;
    text-decoration: none;
    cursor: pointer;
    font-weight: 600;
  }

  .refresh-btn:disabled {
    opacity: 0.65;
    cursor: wait;
  }

  .export-btn {
    background: #0f3a2c;
    border-color: #14532d;
    color: #bbf7d0;
  }

  .notice {
    padding: 0.9rem 1rem;
    border-radius: 0.8rem;
    background: #0f1829;
    border: 1px solid #1e3a5f;
  }

  .notice.error {
    border-color: #7f1d1d;
    color: #fecaca;
  }

  .stats-grid {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 0.9rem;
  }

  .stat-card,
  .panel,
  .state-card {
    background: #0f1829;
    border: 1px solid #1e3a5f;
    border-radius: 0.9rem;
  }

  .stat-card {
    display: grid;
    gap: 0.25rem;
    padding: 1rem;
  }

  .stat-card strong {
    font-size: 1.5rem;
    color: #f8fafc;
  }

  .label {
    color: #7a8da8;
    font-size: 0.74rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .muted {
    color: #94a3b8;
    font-size: 0.84rem;
  }

  .content-grid {
    display: grid;
    grid-template-columns: 1.5fr 1fr 1fr;
    gap: 1rem;
    align-items: start;
  }

  .panel {
    padding: 1rem;
    min-width: 0;
  }

  .panel.wide {
    grid-column: span 3;
  }

  .panel-header {
    display: flex;
    justify-content: space-between;
    align-items: end;
    gap: 0.75rem;
    margin-bottom: 0.85rem;
  }

  .count-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    gap: 0.75rem;
  }

  .count-card {
    display: grid;
    gap: 0.2rem;
    padding: 0.85rem;
    border-radius: 0.75rem;
    background: rgba(8, 47, 73, 0.24);
    border: 1px solid rgba(103, 232, 249, 0.18);
  }

  .count-card strong {
    font-size: 1.4rem;
    color: #cffafe;
  }

  .count-card span {
    color: #cbd5e1;
    line-height: 1.35;
  }

  .mini-list {
    display: grid;
    gap: 0.55rem;
  }

  .mini-row {
    display: flex;
    justify-content: space-between;
    gap: 1rem;
    padding: 0.75rem 0.85rem;
    border-radius: 0.7rem;
    background: rgba(15, 23, 42, 0.92);
  }

  .mini-row strong {
    color: #f8fafc;
  }

  .table-wrap {
    overflow-x: auto;
  }

  table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.92rem;
  }

  th,
  td {
    padding: 0.75rem 0.55rem;
    border-bottom: 1px solid rgba(30, 58, 95, 0.72);
    text-align: left;
  }

  th {
    color: #7a8da8;
    font-size: 0.75rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .events {
    display: grid;
    gap: 0.75rem;
  }

  .event-card {
    display: grid;
    gap: 0.35rem;
    padding: 0.95rem 1rem;
    border-radius: 0.8rem;
    border: 1px solid #1e3a5f;
    background: rgba(15, 23, 42, 0.9);
  }

  .event-card.warn {
    border-color: #92400e;
  }

  .event-card.error {
    border-color: #7f1d1d;
  }

  .event-top,
  .event-meta {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem 0.85rem;
  }

  .event-time,
  .event-level,
  .event-meta {
    color: #94a3b8;
    font-size: 0.82rem;
  }

  .event-card strong {
    color: #f8fafc;
  }

  .empty {
    color: #94a3b8;
    padding: 0.5rem 0;
  }

  .sidebar-header {
    padding: 1rem;
    border-bottom: 1px solid #1e3a5f;
    display: grid;
    gap: 0.35rem;
  }

  .state-card {
    margin: 1rem;
    padding: 0.95rem 1rem;
    display: grid;
    gap: 0.25rem;
  }

  .state-card.success {
    border-color: #166534;
  }

  .state-card.warning {
    border-color: #92400e;
  }

  .state-card.info {
    border-color: #0f766e;
  }

  @media (max-width: 1100px) {
    .stats-grid {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }

    .content-grid {
      grid-template-columns: 1fr;
    }

    .panel.wide {
      grid-column: span 1;
    }
  }

  @media (max-width: 700px) {
    .hero {
      grid-template-columns: 1fr;
      display: grid;
    }

    .stats-grid {
      grid-template-columns: 1fr;
    }
  }
</style>
