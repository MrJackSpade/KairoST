package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.CatalogGame
import com.mrjackspade.kairo.frontend.ControllerGuestSpec
import com.mrjackspade.kairo.frontend.ControllerLayout
import com.mrjackspade.kairo.frontend.DiskImageEntry
import com.mrjackspade.kairo.frontend.DiskImageLibrary
import com.mrjackspade.kairo.frontend.FirmwareSet
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

    override val machineOptions = StCatalog.options
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

    override fun startGame(entry: DiskImageEntry, game: CatalogGame, cancelled: AtomicBoolean,
                           progress: (String) -> Unit) {
        val home = prepareHome(progress)
        val options = optionValues(game).toMutableMap()
        // The player's TOS is the one for the game's model; the core gets its file (st_core.cpp).
        if (options[StCatalog.system.id] == StCatalog.OWN_TOS) {
            val model = StTos.forMachine(options.getValue(StCatalog.machine.id))
            val part = tosParts.getValue(model)
            check(File(tosDirectory, part.name).isFile) {
                "This game starts on your own TOS: import ${part.label} in Menu › Machine"
            }
            options[StCatalog.system.id] = part.name
        }
        // A program runs from its folder, as drive C, with the drives empty.
        val program = entry.takeUnless(::isFloppy)?.let { this.images.prepare(it, cancelled, progress).absolutePath }
        val second = if (program != null) null else game.mediaId("floppyB")?.let { id ->
            mediaFor(id, entry, libraryEntries) ?: error("The second disk this game needs is not in the library")
        }
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
    }

    override fun stateError(code: Int) = when (code) {
        2 -> "the emulator could not read or write the state"
        3 -> "the emulator refused the disk image"
        else -> "code $code"
    }
}
