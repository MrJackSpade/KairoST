package com.mrjackspade.kairost

import android.content.Intent
import android.os.Bundle
import com.mrjackspade.kairo.frontend.CatalogGame
import com.mrjackspade.kairo.frontend.ChoiceOption
import com.mrjackspade.kairo.frontend.ControllerGuestSpec
import com.mrjackspade.kairo.frontend.ControllerLayout
import com.mrjackspade.kairo.frontend.DiskImageEntry
import com.mrjackspade.kairo.frontend.DiskImageLibrary
import com.mrjackspade.kairo.frontend.FirmwareSet
import com.mrjackspade.kairo.frontend.ImportedFilePicker
import com.mrjackspade.kairo.frontend.LayeredGameCatalog
import com.mrjackspade.kairo.frontend.LibraryStrings
import com.mrjackspade.kairo.frontend.MachineActivity
import com.mrjackspade.kairo.frontend.MachineFirstRun
import com.mrjackspade.kairo.frontend.NativeMachine
import com.mrjackspade.kairo.frontend.mediaFor

import java.io.File
import java.util.concurrent.atomic.AtomicBoolean

/** KairoST: the Atari ST on Hatari, in the shared Kairo frontend. */
class MainActivity : MachineActivity<DiskImageEntry>() {
    override val identity = Identity("KAIROST", 0xffFEE902.toInt(), "KairoST", "Atari ST", "kairost",
        LibraryStrings("KAIROST", "KairoST"),
        "KairoST uses Hatari's Atari ST emulation and the EmuTOS system software. Source and provenance: " +
            "github.com/MrJackSpade/KairoST.",
        "Choose a games folder to find floppy disk images, game programs and the ZIP and 7z archives holding them",
        "Folder access expired. Select the games folder again.",
        MachineFirstRun.Folder("Choose a games folder", "Select the folder containing your Atari ST games. KairoST " +
            "will scan its floppy disk images, game programs and the ZIP and 7z archives holding them.", "Choose games folder…"),
        "SAVE STATES", "MrJackSpade/KairoST")
    // The core's picture is low resolution with its borders, doubled (832 x 552); mouse motion is
    // counted in ST low-resolution pixels.
    override val machine = Machine(320, 200, 4.0 / 3.0, "state.sts")
    override val bridge by lazy { NativeMachine("kairost") }
    override val keyboard = StKeys.layout
    override val maxKeyCode = StKeys.MAX_CODE
    override fun guestKey(androidKey: Int) = StKeys.fromAndroid(androidKey)
    override val joystickControls get() = StKeys.joystick
    override val controllerSpec = ControllerGuestSpec("Atari ST", StKeys.bindableCodes, { StKeys.label(it) ?: "Key $it" },
        setOf(StKeys.LEFT_SHIFT, StKeys.RIGHT_SHIFT, StKeys.CONTROL, StKeys.ALTERNATE), StKeys.joystick.zip(StKeys.joystickLabels),
        "The joystick in port 1.", ControllerGuestSpec.STANDARD_ACTIONS, StKeys.layout)
    override val bindingsCodec = StKeys.bindings
    override fun defaultBindings(layout: ControllerLayout) = StKeys.defaults(layout)
    override val artworkDownloadsEnabled: Boolean get() = resources.getBoolean(R.bool.catalog_art_download_enabled)

    /**
     * Floppy disk images in the formats Hatari reads, and programs, which run from their folder as
     * drive C (st_machine.h Config.program).
     */
    private val images by lazy {
        DiskImageLibrary(this, DiskImageLibrary.Format(
            kinds = listOf("st", "msa", "stx", "dim").associateWith { "fd" } +
                listOf("prg", "tos", "ttp", "app", "gtp").associateWith { "prg" },
            labels = mapOf("fd" to "Floppy disk", "prg" to "Program"),
            maxBytes = mapOf("fd" to 16L * 1024 * 1024, "prg" to 16L * 1024 * 1024),
            guestName = "Atari ST",
            programs = setOf("prg")),
            // 2: archives are read again for the programs in them.
            DiskImageLibrary.Cache(mediaFormatsVersion = 2))
    }
    override fun createCatalog() = LayeredGameCatalog(this, StCatalog)
    override fun createLibrary(catalog: LayeredGameCatalog) = images

    // System lists the player's TOS images too, and imports more (StCatalog.system).
    override val machineOptions = StCatalog.options.map { option ->
        if (option.id == StCatalog.system.id) StCatalog.system(::tosChoices, ::tosActions) else option
    }
    override val mediaPageTitle = "Disks"
    private fun isFloppy(entry: DiskImageEntry) = entry.mediaLabel == "Floppy disk"

    // In the core's drive order (st_machine.h Drive).
    override val mediaSlots = listOf(MediaSlot<DiskImageEntry>("Drive A", ::isFloppy), MediaSlot("Drive B", ::isFloppy))

    private val tosDirectory by lazy { File(filesDir, "tos") }

    /** The player's TOS for each model, by the version its header names (StTos). */
    private val tosParts = StTos.Model.entries.associateWith { model ->
        FirmwareSet.Part(model.file, "TOS for the ${model.label}", "${model.file} · ${model.description}",
            setOf(model.file), false, sizes = setOf(model.bytes), identifies = { header -> StTos.rank(header, model) })
    }
    override val firmwareSets by lazy { listOf("TOS" to FirmwareSet(tosDirectory, tosParts.values.toList())) }

    /** Every TOS image the player has given the app, which a game can start on by name. */
    private val tosLibrary by lazy { TosLibrary(tosDirectory) }

    private fun tosChoices() = tosLibrary.images().map { ChoiceOption(it.id, it.label) }

    /** Imports a TOS image into the library and picks it for the game (or all games) asking. */
    private val tosImport by lazy {
        ImportedFilePicker(this, tosLibrary.files, TOS_IMPORT_REQUEST, "tos-import", "Import a TOS image…", "TOS", ::toast)
    }

    private fun tosActions(select: (String) -> Unit) = tosImport.actions(select)

    override fun handleProductResult(requestCode: Int, resultCode: Int, data: Intent?): Boolean =
        tosImport.handle(requestCode, resultCode, data) || super.handleProductResult(requestCode, resultCode, data)

    /** The model rows join the library, so a game that picked one keeps it when the row changes. */
    override fun firmwareImported() = tosLibrary.mirrorRows()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Rows imported before the library kept them; a game's settings list them from the start.
        Thread { tosLibrary.mirrorRows() }.apply { name = "KairoST-tos-library"; start() }
    }

    override fun startGame(entry: DiskImageEntry, game: CatalogGame, cancelled: AtomicBoolean,
                           progress: (String) -> Unit) {
        val home = prepareHome(progress)
        val options = optionValues(game).toMutableMap()
        // The core gets the file of the player's TOS (st_core.cpp): the one for the game's model,
        // or the image the game names.
        tosLibrary.mirrorRows()
        val system = options.getValue(StCatalog.system.id)
        if (system == StCatalog.OWN_TOS) {
            val model = StTos.forMachine(options.getValue(StCatalog.machine.id))
            val part = tosParts.getValue(model)
            check(File(tosDirectory, part.name).isFile) {
                "This game starts on your own TOS: import ${part.label} in Menu › Machine"
            }
            options[StCatalog.system.id] = part.name
        } else if (system != StCatalog.EMUTOS) {
            val file = checkNotNull(tosLibrary.file(system)) {
                "This game's TOS image is no longer in the app: choose or import another in Game settings › System"
            }
            options[StCatalog.system.id] = file.relativeTo(tosDirectory).path
        }
        // A program runs from its folder, as drive C, with the drives empty.
        val program = entry.takeUnless(::isFloppy)?.let { this.images.prepare(it, cancelled, progress).absolutePath }
        val second = if (program != null) null else game.mediaId("floppyB")?.let { id ->
            mediaFor(id, entry, libraryEntries) ?: error("The second disk this game needs is not in the library")
        }
        // The program on drive A that TOS starts, "" for the automatic choice (st_core.cpp).
        options[StCatalog.PROGRAM] = if (program == null) StCatalog.program(game) else ""
        // In the core's order (st_core.cpp): drive A, drive B, then the program.
        val media = listOf(entry.takeIf(::isFloppy), second)
        val images = media.map { image -> image?.let { this.images.prepare(it, cancelled, progress).absolutePath } } + program
        check(bridge.start(tosDirectory.absolutePath, home.absolutePath, images, options)) { "Another machine is still running" }
        awaitRunning()
        mediaStarted(media)
    }

    /**
     * The core's home (st_core.cpp): "emutos", EmuTOS copied from assets/emutos when it differs
     * from the app's,
     * and "hatari", Hatari's own files.
     */
    private fun prepareHome(progress: (String) -> Unit): File {
        val home = File(filesDir, "machine")
        val emutos = File(home, "emutos/$EMUTOS_IMAGE")
        val shipped = assets.open("emutos/$EMUTOS_IMAGE").use { it.readBytes() }
        if (!emutos.isFile || !emutos.readBytes().contentEquals(shipped)) {
            progress("Preparing the emulator")
            check(emutos.parentFile!!.mkdirs() || emutos.parentFile!!.isDirectory) { "Could not create ${emutos.parentFile}" }
            val copy = File(emutos.parentFile, "$EMUTOS_IMAGE.new")
            copy.writeBytes(shipped)
            check(copy.renameTo(emutos)) { "Could not install EmuTOS in $emutos" }
        }
        File(home, "hatari").mkdirs()
        return home
    }

    private companion object {
        /** EmuTOS for every ST, STE, TT and Falcon, in every language (third_party/emutos-1024k). */
        const val EMUTOS_IMAGE = "etos1024k.img"
        /** The picker of a TOS image for a game's System (tosActions). */
        const val TOS_IMPORT_REQUEST = FIRST_PRODUCT_REQUEST
    }

    override fun stateError(code: Int) = when (code) {
        2 -> "the emulator could not read or write the state"
        3 -> "the emulator refused the disk image"
        else -> "code $code"
    }
}
