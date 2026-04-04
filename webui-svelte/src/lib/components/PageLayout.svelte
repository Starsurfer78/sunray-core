<script lang="ts">
    import { createEventDispatcher } from "svelte";

    export let sidebarCollapsed = false;

    const dispatch = createEventDispatcher();
</script>

<main class="page">
    <section class="layout" class:collapsed={sidebarCollapsed}>
        <div class="main-column">
            <div class="main-content">
                <slot />
            </div>
            <slot name="bottom" />
        </div>

        <aside class="sidebar" class:collapsed={sidebarCollapsed}>
            <button
                class="collapse-btn"
                class:collapsed={sidebarCollapsed}
                title={sidebarCollapsed
                    ? "Sidebar ausklappen"
                    : "Sidebar einklappen"}
                on:click={() => dispatch("toggle")}
            >
                <span class="collapse-icon">{sidebarCollapsed ? "›" : "‹"}</span
                >
            </button>

            <div class="sr-side" class:hidden={sidebarCollapsed}>
                <slot name="sidebar" />
            </div>
        </aside>
    </section>
</main>

<style>
    .page {
        height: 100%;
    }

    .layout {
        position: relative;
        height: 100%;
        overflow: hidden;
    }

    .layout.collapsed .main-column {
        margin-right: 0;
    }

    .main-column {
        min-width: 0;
        height: 100%;
        margin-right: 275px;
        transition: margin-right 0.2s;
        display: grid;
        grid-template-rows: 1fr auto;
    }

    .main-content {
        min-height: 0;
        overflow: hidden;
    }

    .sidebar {
        position: absolute;
        top: 0;
        right: 0;
        bottom: 0;
        width: 275px;
        z-index: 3;
        padding-left: 0.55rem;
    }

    .sidebar.collapsed {
        width: 0;
    }

    .sr-side {
        display: flex;
        flex-direction: column;
        background: #0a1020;
        border: 1px solid #1e3a5f;
        border-radius: 0.75rem;
        overflow-y: auto;
        overflow-x: hidden;
        height: 100%;
        box-shadow: 0 18px 40px rgba(0, 0, 0, 0.28);
    }

    .sr-side.hidden {
        display: none;
    }

    .collapse-btn {
        position: absolute;
        left: calc(0.55rem - 14px);
        top: 50%;
        transform: translateY(-50%);
        z-index: 10;
        width: 14px;
        height: 44px;
        background: #0f1829 !important;
        border: 1px solid #1e3a5f;
        border-right: none;
        color: #475569 !important;
        cursor: pointer;
        border-radius: 6px 0 0 6px;
        font-size: 14px;
        display: flex;
        align-items: center;
        justify-content: center;
        padding: 0;
        transition: color 0.2s;
    }

    .collapse-btn:hover {
        color: #60a5fa !important;
    }

    .collapse-btn.collapsed {
        left: calc(0.55rem - 14px);
        border-right: 1px solid #1e3a5f;
        border-left: none;
        border-radius: 0 6px 6px 0;
    }

    .collapse-icon {
        display: inline-block;
    }

    @media (max-width: 640px) {
        .main-column {
            margin-right: 0 !important;
        }

        .sidebar {
            display: none;
        }

        .collapse-btn {
            display: none;
        }
    }
</style>
