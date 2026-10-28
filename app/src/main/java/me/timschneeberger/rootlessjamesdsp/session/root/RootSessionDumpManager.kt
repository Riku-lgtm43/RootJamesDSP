package me.timschneeberger.rootlessjamesdsp.session.root

import android.content.*
import android.media.AudioAttributes
import android.media.AudioPlaybackConfiguration
import android.media.AudioPlaybackConfigurationHidden
import dev.rikka.tools.refine.Refine
import me.timschneeberger.rootlessjamesdsp.model.AudioSessionDumpEntry
import me.timschneeberger.rootlessjamesdsp.session.dump.DumpManager
import me.timschneeberger.rootlessjamesdsp.session.dump.data.ISessionInfoDump
import me.timschneeberger.rootlessjamesdsp.session.shared.BaseSessionManager
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.getPackageNameFromUid


class RootSessionDumpManager(context: Context) : BaseSessionManager(context)
{
    private var onSessionDump: ((sessionDump: ISessionInfoDump) -> Unit)? = null
    private var onPlaybackSessions: ((HashMap<Int, AudioSessionDumpEntry>) -> Unit)? = null
    private var onDumpMethodChanged: (() -> Unit)? = null

    fun setOnSessionDump(callback: ((sessionDump: ISessionInfoDump) -> Unit)?) {
        onSessionDump = callback
    }

    fun setOnPlaybackSessions(callback: ((HashMap<Int, AudioSessionDumpEntry>) -> Unit)?) {
        onPlaybackSessions = callback
    }

    fun setOnDumpMethodChanged(callback: (() -> Unit)?) {
        onDumpMethodChanged = callback
    }

    override fun handleSessionDump(sessionDump: ISessionInfoDump?) {
        sessionDump?.let { onSessionDump?.invoke(it) }
    }

    override fun onPlaybackConfigurationsChanged(
        configs: MutableList<AudioPlaybackConfiguration>?
    ) {
        if (configs == null) {
            pollOnce(false)
            return
        }

        val sessions = hashMapOf<Int, AudioSessionDumpEntry>()
        var snapshotComplete = true
        configs.forEach { config ->
            val playback = try {
                Refine.unsafeCast<AudioPlaybackConfigurationHidden>(config)
            } catch (_: Exception) {
                snapshotComplete = false
                return@forEach
            }

            val sessionId = try {
                playback.sessionId
            } catch (_: Exception) {
                snapshotComplete = false
                return@forEach
            }
            val uid = try {
                playback.clientUid
            } catch (_: Exception) {
                snapshotComplete = false
                return@forEach
            }
            if (sessionId <= 0 || uid < 0) {
                snapshotComplete = false
                return@forEach
            }

            val attributes = config.audioAttributes
            val usage = attributes?.let { AudioAttributes.usageToString(it.usage) } ?: "USAGE_UNKNOWN"
            val content = attributes?.let { AudioAttributes.contentTypeToString(it.contentType) }
                ?: "CONTENT_TYPE_UNKNOWN"
            val packageName = context.getPackageNameFromUid(uid) ?: uid.toString()
            sessions[sessionId] = AudioSessionDumpEntry(uid, packageName, usage, content)
        }

        if (snapshotComplete) {
            onPlaybackSessions?.invoke(sessions)
        }
        pollOnce(false)
    }

    override fun onDumpMethodChange(method: DumpManager.Method) {
        onDumpMethodChanged?.invoke()
        super.onDumpMethodChange(method)
    }
}
