package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.toHex
import java.io.File
import java.io.InputStream
import java.security.MessageDigest

/**
 * Every TOS image the player has given KairoST, kept by content in [directory]/library as
 * <hash>.img, for games that ask for one by name (StCatalog.system, "tos:<hash>"). The model rows
 * of the firmware page ([directory]/tos-st.img and the others) are copied in too, so a game keeps
 * its TOS when a row is replaced.
 */
class TosLibrary(private val directory: File) {
    class Image(val id: String, val file: File, val label: String, val version: Int)

    private val library = File(directory, "library")
    /** The images as last read; settings pages read them for every row they draw. */
    @Volatile private var cached: List<Image>? = null

    /** The images, oldest TOS first. */
    fun images(): List<Image> = cached ?: read().also { cached = it }

    private fun read(): List<Image> = (library.listFiles { file -> file.name.endsWith(".img") } ?: emptyArray())
        .map { file ->
            val header = file.inputStream().use(StTos::readHeader)
            Image(ID_PREFIX + file.nameWithoutExtension, file, StTos.describe(header), StTos.version(header))
        }.sortedWith(compareBy<Image> { it.version }.thenBy { it.label })

    /** The image a game names by [id], or null when it is not in the library. */
    fun file(id: String): File? = File(library, "${id.removePrefix(ID_PREFIX)}.img").takeIf { it.isFile }

    /** Copies the model rows into the library, each once. */
    fun mirrorRows() {
        for (model in StTos.Model.entries) {
            val row = File(directory, model.file)
            if (row.isFile) row.inputStream().use(::add)
        }
    }

    /** Adds the image read from [input] and returns it; one already in the library is kept as it is. */
    @Synchronized fun add(input: InputStream): Image {
        check(library.isDirectory || library.mkdirs()) { "Cannot create $library" }
        val partial = File(library, "import.part")
        val digest = MessageDigest.getInstance("SHA-1")
        try {
            partial.outputStream().use { output ->
                val buffer = ByteArray(64 * 1024)
                var size = 0L
                while (true) {
                    val count = input.read(buffer)
                    if (count < 0) break
                    size += count
                    require(size <= MAX_BYTES) { "A TOS image is at most 1 MB; that file is larger" }
                    digest.update(buffer, 0, count)
                    output.write(buffer, 0, count)
                }
                require(size > 0) { "That file is empty" }
            }
            val id = digest.digest().copyOf(8).toHex()
            val file = File(library, "$id.img")
            if (file.isFile) partial.delete()
            else check(partial.renameTo(file)) { "Could not save the TOS image" }
            cached = null
            return images().first { it.id == ID_PREFIX + id }
        } finally {
            partial.delete()
        }
    }

    companion object {
        /** A game's choice of one image: "tos:" and its hash. */
        const val ID_PREFIX = "tos:"
        val ID = Regex("tos:[0-9a-f]{16}")
        /** EmuTOS's largest build, the biggest TOS-sized image a player may bring. */
        private const val MAX_BYTES = 1024L * 1024
    }
}
