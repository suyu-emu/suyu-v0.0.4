// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.yuzu.yuzu_emu.fragments.DriverFetcherFragment
import java.io.File
import java.io.IOException
import java.util.concurrent.TimeUnit
import java.util.concurrent.ConcurrentHashMap
import okhttp3.ConnectionPool
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import kotlinx.coroutines.delay
import kotlin.math.pow

/**
 * Resolves driver download URLs from filenames by searching GitHub repositories
 */
object DriverResolver {
    private const val CONNECTION_TIMEOUT_SECONDS = 30L
    private const val CACHE_DURATION_MS = 3600000L // 1 hour
    private const val BUFFER_SIZE = 8192
    private const val MIN_API_CALL_INTERVAL = 2000L // 2 seconds between API calls
    private const val MAX_RETRY_COUNT = 3

    @Volatile
    private var client: OkHttpClient? = null

    private fun getClient(): OkHttpClient {
        return client ?: synchronized(this) {
            client ?: OkHttpClient.Builder()
                .connectTimeout(CONNECTION_TIMEOUT_SECONDS, TimeUnit.SECONDS)
                .readTimeout(CONNECTION_TIMEOUT_SECONDS, TimeUnit.SECONDS)
                .writeTimeout(CONNECTION_TIMEOUT_SECONDS, TimeUnit.SECONDS)
                .followRedirects(true)
                .followSslRedirects(true)
                .connectionPool(ConnectionPool(5, 1, TimeUnit.MINUTES))
                .build().also { client = it }
        }
    }

    // Driver repository paths - (from DriverFetcherFragment) might extract these to a config file later
    private val repositories = listOf(
        "MrPurple666/purple-turnip",
        "crueter/GameHub-8Elite-Drivers",
        "K11MCH1/AdrenoToolsDrivers",
        "Weab-chan/freedreno_turnip-CI"
    )

    private val urlCache = ConcurrentHashMap<String, ResolvedDriver>()
    private val releaseCache = ConcurrentHashMap<String, List<DriverFetcherFragment.Release>>()
    private var lastCacheTime = 0L
    private var lastApiCallTime = 0L

    data class ResolvedDriver(
        val downloadUrl: String,
        val repoPath: String,
        val releaseTag: String,
        val filename: String
    )

    // Matching helpers
    private val KNOWN_SUFFIXES = listOf(
        ".adpkg.zip",
        ".zip",
        ".7z",
        ".tar.gz",
        ".tar.xz",
        ".rar"
    )

    private fun stripKnownSuffixes(name: String): String {
        var result = name
        var changed: Boolean
        do {
            changed = false
            for (s in KNOWN_SUFFIXES) {
                if (result.endsWith(s, ignoreCase = true)) {
                    result = result.dropLast(s.length)
                    changed = true
                }
            }
        } while (changed)
        return result
    }

    private fun normalizeName(name: String): String {
        val base = stripKnownSuffixes(name.lowercase())
        // Remove non-alphanumerics to make substring checks resilient
        return base.replace(Regex("[^a-z0-9]+"), " ").trim()
    }

    private fun tokenize(name: String): Set<String> =
        normalizeName(name).split(Regex("\\s+")).filter { it.isNotBlank() }.toSet()

    // Jaccard similarity between two sets
    private fun jaccard(a: Set<String>, b: Set<String>): Double {
        if (a.isEmpty() || b.isEmpty()) return 0.0
        val inter = a.intersect(b).size.toDouble()
        val uni = a.union(b).size.toDouble()
        return if (uni == 0.0) 0.0 else inter / uni
    }

    /**
     * Resolve a driver download URL from its filename
     * @param filename The driver filename (e.g., "turnip_mrpurple-T19-toasted.adpkg.zip")
     * @return ResolvedDriver with download URL and metadata, or null if not found
     */
    suspend fun resolveDriverUrl(filename: String): ResolvedDriver? {
        // Validate input
        require(filename.isNotBlank()) { "Filename cannot be blank" }
        require(!filename.contains("..")) { "Invalid filename: path traversal detected" }

        // Check cache first
        urlCache[filename]?.let {
            Log.info("[DriverResolver] Found cached URL for $filename")
            return it
        }

        Log.info("[DriverResolver] Resolving download URL for: $filename")

        // Clear cache if expired
        if (System.currentTimeMillis() - lastCacheTime > CACHE_DURATION_MS) {
            releaseCache.clear()
            lastCacheTime = System.currentTimeMillis()
        }

        return coroutineScope {
            // Search all repositories in parallel
            repositories.map { repoPath ->
                async {
                    searchRepository(repoPath, filename)
                }
            }.firstNotNullOfOrNull { it.await() }.also { resolved ->
                // Cache the result if found
                resolved?.let {
                    urlCache[filename] = it
                    Log.info("[DriverResolver] Cached resolution for $filename from ${it.repoPath}")
                }
            }
        }
    }

    /**
     * Search a specific repository for a driver file
     */
    private suspend fun searchRepository(repoPath: String, filename: String): ResolvedDriver? {
        return withContext(Dispatchers.IO) {
            try {
                // Get releases from cache or fetch
                val releases = releaseCache[repoPath] ?: fetchReleases(repoPath).also {
                    releaseCache[repoPath] = it
                }

                // First pass: exact name (case-insensitive) against asset filenames
                val target = filename.lowercase()
                for (release in releases) {
                    for (artifact in release.artifacts) {
                        if (artifact.name.equals(filename, ignoreCase = true) || artifact.name.lowercase() == target) {
                            Log.info("[DriverResolver] Found $filename in $repoPath/${release.tagName}")
                            return@withContext ResolvedDriver(
                                downloadUrl = artifact.url.toString(),
                                repoPath = repoPath,
                                releaseTag = release.tagName,
                                filename = artifact.name
                            )
                        }
                    }
                }

                // Second pass: fuzzy match by asset filenames only
                val reqNorm = normalizeName(filename)
                val reqTokens = tokenize(filename)
                var best: ResolvedDriver? = null
                var bestScore = 0.0

                for (release in releases) {
                    for (artifact in release.artifacts) {
                        val artNorm = normalizeName(artifact.name)
                        val artTokens = tokenize(artifact.name)

                        var score = jaccard(reqTokens, artTokens)
                        // Boost if one normalized name contains the other
                        if (artNorm.contains(reqNorm) || reqNorm.contains(artNorm)) {
                            score = maxOf(score, 0.92)
                        }

                        if (score > bestScore) {
                            bestScore = score
                            best = ResolvedDriver(
                                downloadUrl = artifact.url.toString(),
                                repoPath = repoPath,
                                releaseTag = release.tagName,
                                filename = artifact.name
                            )
                        }
                    }
                }

                // Threshold to avoid bad guesses, this worked fine in testing but might need tuning
                if (best != null && bestScore >= 0.6) {
                    Log.info("[DriverResolver] Fuzzy matched $filename -> ${best.filename} in ${best.repoPath} (score=%.2f)".format(bestScore))
                    return@withContext best
                }
                null
            } catch (e: Exception) {
                Log.error("[DriverResolver] Failed to search $repoPath: ${e.message}")
                null
            }
        }
    }

    /**
     * Fetch releases from a GitHub repository
     */
    private suspend fun fetchReleases(repoPath: String): List<DriverFetcherFragment.Release> = withContext(
        Dispatchers.IO
    ) {
        // Rate limiting
        val timeSinceLastCall = System.currentTimeMillis() - lastApiCallTime
        if (timeSinceLastCall < MIN_API_CALL_INTERVAL) {
            delay(MIN_API_CALL_INTERVAL - timeSinceLastCall)
        }
        lastApiCallTime = System.currentTimeMillis()

        // Retry logic with exponential backoff
        var retryCount = 0
        var lastException: Exception? = null

        while (retryCount < MAX_RETRY_COUNT) {
            try {
                val request = Request.Builder()
                    .url("https://api.github.com/repos/$repoPath/releases")
                    .header("Accept", "application/vnd.github.v3+json")
                    .build()

                return@withContext getClient().newCall(request).execute().use { response ->
                    when {
                        response.code == 404 -> throw IOException("Repository not found: $repoPath")
                        response.code == 403 -> {
                            val resetTime = response.header("X-RateLimit-Reset")?.toLongOrNull() ?: 0
                            throw IOException(
                                "API rate limit exceeded. Resets at ${java.util.Date(
                                    resetTime * 1000
                                )}"
                            )
                        }
                        !response.isSuccessful -> throw IOException(
                            "HTTP ${response.code}: ${response.message}"
                        )
                    }

                    val body = response.body?.string()
                        ?: throw IOException("Empty response from $repoPath")

                    // Determine if this repo uses tag names (from DriverFetcherFragment logic)
                    val useTagName = repoPath.contains("K11MCH1")
                    val sortMode = if (useTagName) {
                        DriverFetcherFragment.SortMode.PublishTime
                    } else {
                        DriverFetcherFragment.SortMode.Default
                    }

                    DriverFetcherFragment.Release.fromJsonArray(body, useTagName, sortMode)
                }
            } catch (e: IOException) {
                lastException = e
                if (retryCount == MAX_RETRY_COUNT - 1) throw e
                delay((2.0.pow(retryCount) * 1000).toLong())
                retryCount++
            }
        }
        throw lastException ?: IOException(
            "Failed to fetch releases after $MAX_RETRY_COUNT attempts"
        )
    }

    /**
     * Download a driver file to the cache directory
     * @param resolvedDriver The resolved driver information
     * @param context Android context for cache directory
     * @return The downloaded file, or null if download failed
     */
    suspend fun downloadDriver(
        resolvedDriver: ResolvedDriver,
        context: Context,
        onProgress: ((Float) -> Unit)? = null
    ): File? {
        return withContext(Dispatchers.IO) {
            try {
                Log.info(
                    "[DriverResolver] Downloading ${resolvedDriver.filename} from ${resolvedDriver.repoPath}"
                )

                val cacheDir = context.externalCacheDir ?: throw IOException("Failed to access cache directory")
                cacheDir.mkdirs()

                val file = File(cacheDir, resolvedDriver.filename)

                // If file already exists in cache and has content, return it
                if (file.exists() && file.length() > 0) {
                    Log.info("[DriverResolver] Using cached file: ${file.absolutePath}")
                    return@withContext file
                }

                val request = Request.Builder()
                    .url(resolvedDriver.downloadUrl)
                    .header("Accept", "application/octet-stream")
                    .build()

                getClient().newCall(request).execute().use { response ->
                    if (!response.isSuccessful) {
                        throw IOException("Download failed: ${response.code}")
                    }

                    response.body?.use { body ->
                        val contentLength = body.contentLength()
                        body.byteStream().use { input ->
                            file.outputStream().use { output ->
                                val buffer = ByteArray(BUFFER_SIZE)
                                var totalBytesRead = 0L
                                var bytesRead: Int

                                while (input.read(buffer).also { bytesRead = it } != -1) {
                                    output.write(buffer, 0, bytesRead)
                                    totalBytesRead += bytesRead

                                    if (contentLength > 0) {
                                        val progress = (totalBytesRead.toFloat() / contentLength) * 100f
                                        onProgress?.invoke(progress)
                                    }
                                }
                            }
                        }
                    } ?: throw IOException("Empty response body")
                }

                if (file.length() == 0L) {
                    file.delete()
                    throw IOException("Downloaded file is empty")
                }

                Log.info(
                    "[DriverResolver] Successfully downloaded ${file.length()} bytes to ${file.absolutePath}"
                )
                file
            } catch (e: Exception) {
                Log.error("[DriverResolver] Download failed: ${e.message}")
                null
            }
        }
    }

    /**
     * Download and install a driver if not already present
     * @param driverPath The driver filename or full path
     * @param context Android context
     * @param onProgress Optional progress callback (0-100)
     * @return Uri of the installed driver, or null if failed
     */
    suspend fun ensureDriverAvailable(
        driverPath: String,
        context: Context,
        onProgress: ((Float) -> Unit)? = null
    ): Uri? {
        // Extract filename from path (support both separators)
        val filename = driverPath.substringAfterLast('/').substringAfterLast('\\')

        // Check if driver already exists locally
        val localPath = "${GpuDriverHelper.driverStoragePath}$filename"
        val localFile = File(localPath)

        if (localFile.exists() && localFile.length() > 0) {
            Log.info("[DriverResolver] Driver already exists locally: $localPath")
            return Uri.fromFile(localFile)
        }

        Log.info("[DriverResolver] Driver not found locally, attempting to download: $filename")

        // Resolve download URL
        val resolvedDriver = resolveDriverUrl(filename)
        if (resolvedDriver == null) {
            Log.error("[DriverResolver] Failed to resolve download URL for $filename")
            return null
        }

        // Download the driver with progress callback
        val downloadedFile = downloadDriver(resolvedDriver, context, onProgress)
        if (downloadedFile == null) {
            Log.error("[DriverResolver] Failed to download driver $filename")
            return null
        }

        // Install the driver to internal storage
        val downloadedUri = Uri.fromFile(downloadedFile)
        if (GpuDriverHelper.copyDriverToInternalStorage(downloadedUri)) {
            Log.info("[DriverResolver] Successfully installed driver to internal storage")
            // Clean up cache file
            downloadedFile.delete()
            return Uri.fromFile(File(localPath))
        } else {
            Log.error("[DriverResolver] Failed to copy driver to internal storage")
            downloadedFile.delete()
            return null
        }
    }

    /**
     * Check network connectivity
     */
    fun isNetworkAvailable(context: Context): Boolean {
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
            ?: return false
        val network = connectivityManager.activeNetwork ?: return false
        val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return false
        return capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
    }

    /**
     * Clear all caches
     */
    fun clearCache() {
        urlCache.clear()
        releaseCache.clear()
        lastCacheTime = 0L
        lastApiCallTime = 0L
    }

    /**
     * Clean up resources
     */
    fun cleanup() {
        client?.dispatcher?.executorService?.shutdown()
        client?.connectionPool?.evictAll()
        client = null
        clearCache()
    }
}
