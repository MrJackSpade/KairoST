package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.ImportedFiles
import java.io.File
import java.io.InputStream

/**
 * Every TOS image the player has given KairoST, kept by content in [directory]/library
 * (ImportedFiles, "tos:<hash>"), for games that ask for one by name (StCatalog.system). The model
 * rows of the firmware page ([directory]/tos-st.img and the others) are copied in too, so a game
 * keeps its TOS when a row is replaced.
 */
class TosLibrary(private val directory: File) {
    /** The images, oldest TOS first, each named by its header. */
    val files = ImportedFiles(File(directory, "library"), PREFIX, ".img", MAX_BYTES,
        "A TOS image is at most 1 MB; that file is larger") { file, _ ->
        val header = file.inputStream().use(StTos::readHeader)
        ImportedFiles.Described(StTos.describe(header), StTos.version(header))
    }

    fun images(): List<ImportedFiles.Item> = files.items()

    /** The image a game names by [id], or null when it is not in the library. */
    fun file(id: String): File? = files.file(id)

    /** Copies the model rows into the library, each once. */
    fun mirrorRows() {
        for (model in StTos.Model.entries) {
            val row = File(directory, model.file)
            if (row.isFile) row.inputStream().use(::add)
        }
    }

    /** Adds the image read from [input] and returns it; one already in the library is kept as it is. */
    fun add(input: InputStream): ImportedFiles.Item = files.add(input)

    companion object {
        private const val PREFIX = "tos"
        /** A game's choice of one image: "tos:" and its hash. */
        val ID = Regex("tos:[0-9a-f]{16}")
        /** EmuTOS's largest build, the biggest TOS-sized image a player may bring. */
        private const val MAX_BYTES = 1024L * 1024
    }
}
