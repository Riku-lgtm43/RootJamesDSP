package me.timschneeberger.rootlessjamesdsp.session.root

import android.content.Context
import android.content.Intent
import android.media.audiofx.AudioEffect
import kotlinx.coroutines.Job
import me.timschneeberger.rootlessjamesdsp.MainApplication
import me.timschneeberger.rootlessjamesdsp.interop.JamesDspRemoteEngine
import me.timschneeberger.rootlessjamesdsp.interop.ProcessorMessageHandler
import me.timschneeberger.rootlessjamesdsp.model.AudioSessionDumpEntry
import me.timschneeberger.rootlessjamesdsp.model.IEffectSession
import me.timschneeberger.rootlessjamesdsp.model.root.RemoteEffectSession
import me.timschneeberger.rootlessjamesdsp.session.dump.data.AudioServiceDump
import me.timschneeberger.rootlessjamesdsp.session.dump.data.ISessionInfoDump
import me.timschneeberger.rootlessjamesdsp.session.shared.BaseSessionDatabase
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.getUidFromPackage
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.toast
import timber.log.Timber


class RootSessionDatabase(context: Context) : BaseSessionDatabase(context) {

    /**
     * The system dump providers do not expose every active playback session on every device.
     * Keep explicit AudioEffect session events as an overlay so a partial dump cannot remove
     * an app's effect while it is still playing.
     */
    private val enhancedSessionLock = Any()
    private val enhancedIntentSessions = hashMapOf<Int, AudioSessionDumpEntry>()
    private val enhancedClosedSessionIds = mutableSetOf<Int>()
    private var lastEnhancedDumpSessions = hashMapOf<Int, AudioSessionDumpEntry>()

    private val app
        get() = context.applicationContext as MainApplication

    var enabled: Boolean = true
        set(value) {
            field = value
            sessionList.forEach { (_, session) ->
                (session as RemoteEffectSession).effect?.enabled = value
            }
        }

    override fun createSession(id: Int, uid: Int, packageName: String): IEffectSession? {
        if (sessionList[id] == null) {
            Timber.d("Creating effect for session $id (${packageName})")

            val effect = try {
                JamesDspRemoteEngine(context, id, 0, ProcessorMessageHandler())
            }
            catch (ex: Exception) {
                Timber.e("Failed to instantiate JamesDSP effect for session $id ($packageName)")
                Timber.e(ex.cause)

                // Debug toast
                context.toast("JDSP load fail (session=$id): " + ex.message)
                return null
            }

            effect.syncWithPreferences()
            effect.enabled = enabled

            val finalUid = if (uid >= 0)
                uid
            else
                context.getUidFromPackage(packageName)

            return RemoteEffectSession(packageName, finalUid, effect)
        } else {
            Timber.w("Session $id ($packageName) already existed")
            return null
        }
    }

    override fun shouldAcceptSessionDump(id: Int, session: AudioSessionDumpEntry): Boolean {
        if (!session.isUsageRecordable()) {
            Timber.d("Skipped session $id due to usage ($session)")
            return false
        }
        return true
    }
    override fun shouldAddSession(id: Int, uid: Int, packageName: String) = true

    override fun onSessionRemoved(item: IEffectSession) {
        (item as RemoteEffectSession).run {
            item.coroutineContext[Job]!!.invokeOnCompletion {
                item.effect?.close()
            }
            item.coroutineContext[Job]?.cancel()
        }
    }

    fun addSessionByIntent(intent: Intent) {
        val sessionId = intent.getIntExtra(AudioEffect.EXTRA_AUDIO_SESSION, AudioEffect.ERROR)
        if (sessionId < 0 || (app.isLegacyMode && sessionId > 0)) {
            return
        }
        // Legacy mode enabled, remove other sessions
        if (sessionId == 0)
            clearSessions()

        val packageName = intent.getStringExtra(AudioEffect.EXTRA_PACKAGE_NAME) ?: "unknown"

        addSession(
            sessionId,
            context.getUidFromPackage(packageName),
            packageName,
            true
        )
    }

    fun removeSessionByIntent(intent: Intent) {
        val sessionId = intent.getIntExtra(AudioEffect.EXTRA_AUDIO_SESSION, AudioEffect.ERROR)
        val packageName = intent.getStringExtra(AudioEffect.EXTRA_PACKAGE_NAME)
        if (app.isLegacyMode) {
            return
        }

        if (app.isEnhancedProcessing) {
            val sessionIds = if (sessionId > 0) {
                synchronized(enhancedSessionLock) {
                    val knownPackage = enhancedIntentSessions[sessionId]?.packageName
                        ?: lastEnhancedDumpSessions[sessionId]?.packageName
                        ?: sessionList[sessionId]?.packageName
                    if (packageName != null && knownPackage != null && knownPackage != packageName)
                        emptySet()
                    else
                        setOf(sessionId)
                }
            } else if (sessionId == 0 && !packageName.isNullOrBlank()) {
                synchronized(enhancedSessionLock) {
                    (lastEnhancedDumpSessions.filterValues { it.packageName == packageName }.keys +
                            enhancedIntentSessions.filterValues { it.packageName == packageName }.keys +
                            sessionList.filterValues { it.packageName == packageName }.keys).toSet()
                }
            } else {
                emptySet()
            }

            synchronized(enhancedSessionLock) {
                sessionIds.forEach { id ->
                    val wasTracked = enhancedIntentSessions.containsKey(id) ||
                            lastEnhancedDumpSessions.containsKey(id) || sessionList.containsKey(id)
                    enhancedIntentSessions.remove(id)
                    // A dump can briefly report a closed session. Suppress it until a later
                    // successful dump confirms that the id has disappeared.
                    if (wasTracked) enhancedClosedSessionIds.add(id)
                }
            }
            sessionIds.forEach(::removeSession)
            return
        }

        // Fix: some apps submit sid 0 sending the CLOSE broadcast
        if(packageName != null && sessionId == 0) {
            // Remove by package name instead
            sessionList.filter { it.value.packageName == packageName }
                .keys
                .forEach(::removeSession)
        }
        else {
            removeSession(sessionId)
        }
    }

    /** Add an explicit app session without replacing its other active sessions. */
    fun addEnhancedSessionByIntent(intent: Intent) {
        val sessionId = intent.getIntExtra(AudioEffect.EXTRA_AUDIO_SESSION, AudioEffect.ERROR)
        val packageName = intent.getStringExtra(AudioEffect.EXTRA_PACKAGE_NAME)
            ?.takeIf { it.isNotBlank() }

        // Session 0 is the global effect chain and is reserved for legacy mode. Enhanced mode
        // must attach only to a real app playback session.
        if (sessionId <= 0 || packageName == null || packageName == context.packageName) {
            return
        }

        val dumpEntry = synchronized(enhancedSessionLock) {
            lastEnhancedDumpSessions[sessionId]?.takeIf { it.packageName == packageName }
        }
        val packageUid = context.getUidFromPackage(packageName)
        val uid = packageUid.takeIf { it >= 0 } ?: dumpEntry?.uid ?: -1
        val entry = AudioSessionDumpEntry(
            uid,
            packageName,
            "USAGE_UNKNOWN",
            "CONTENT_TYPE_UNKNOWN"
        )
        synchronized(enhancedSessionLock) {
            enhancedClosedSessionIds.remove(sessionId)
            enhancedIntentSessions[sessionId] = entry
        }

        val existing = sessionList[sessionId]
        if (existing != null && existing.packageName != packageName) {
            removeSession(sessionId)
        }
        if (sessionList[sessionId] == null) {
            addSession(sessionId, uid, packageName)
        }
    }

    /**
     * Apply the normal system dump plus explicit session events. Closed-session tombstones
     * prevent a stale system snapshot from immediately recreating an effect after CLOSE.
     */
    fun updateEnhanced(dump: ISessionInfoDump) {
        val mergedSessions = HashMap(dump.sessions)
        synchronized(enhancedSessionLock) {
            enhancedClosedSessionIds.removeAll { !dump.sessions.containsKey(it) }
            enhancedClosedSessionIds.forEach { id -> mergedSessions.remove(id) }
            enhancedIntentSessions.forEach { (id, entry) ->
                val systemEntry = dump.sessions[id]
                mergedSessions[id] = if (systemEntry != null &&
                    (entry.uid < 0 || systemEntry.uid == entry.uid)) {
                    // Keep the intent's package identity, but retain the system dump's UID and
                    // usage classification when it has a matching entry. This preserves app
                    // exclusions and avoids treating a known non-media stream as USAGE_UNKNOWN.
                    entry.copy(
                        uid = systemEntry.uid,
                        usage = systemEntry.usage,
                        content = systemEntry.content
                    )
                } else {
                    entry
                }
            }
            lastEnhancedDumpSessions = HashMap(dump.sessions)
        }

        // BaseSessionDatabase normally removes only IDs missing from the dump. If an existing
        // session changes to a usage we intentionally skip, detach it before applying the dump.
        sessionList.filter { (id, _) ->
            mergedSessions[id]?.let { !shouldAcceptSessionDump(id, it) } == true
        }.keys.toList().forEach(::removeSession)

        super.update(AudioServiceDump(mergedSessions))
    }

    fun clearEnhancedSessionIntents() {
        synchronized(enhancedSessionLock) {
            enhancedIntentSessions.clear()
            enhancedClosedSessionIds.clear()
            lastEnhancedDumpSessions.clear()
        }
    }
}

typealias OnRootSessionChangeListener = BaseSessionDatabase.OnSessionChangeListener
