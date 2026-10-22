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
    override val identity = Identity("KAIROST", 0xff02FA9E.toInt(), "KairoST", "Atari ST", "kairost",
        LibraryStrings("KAIROST", "KairoST"),
        "KairoST uses Hatari's Atari ST emulation and the EmuTOS system software. Source and provenance: " +
            "github.com/MrJackSpade/KairoST.",
        "Choose a games folder to find floppy disk images and the ZIP and 7z archives holding them",
        "Folder access expired. Select the games folder again.",
        MachineFirstRun.Folder("Choose a games folder", "Select the folder containing your Atari ST games. KairoST " +
            "will scan its floppy disk images and the ZIP and 7z archives holding them.", "Choose games folder…"),
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

    /** Floppy disk images in the formats Hatari reads. */
    private val images by lazy {
        DiskImageLibrary(this, DiskImageLibrary.Format(
            kinds = listOf("st", "msa", "stx", "dim").associateWith { "fd" },
            labels = mapOf("fd" to "Floppy disk"),
            maxBytes = mapOf("fd" to 16L * 1024 * 1024),
            guestName = "Atari ST"))
    }
    override fun createCatalog() = LayeredGameCatalog(this, StCatalog)
    override fun createLibrary(catalog: LayeredGameCatalog) = images

    override val machineOptions = StCatalog.options
    override val mediaPageTitle = "Disks"
    // In the core's drive order (st_machine.h Drive).
    override val mediaSlots = listOf(MediaSlot<DiskImageEntry>("Drive A"), MediaSlot("Drive B"))

    private val tosDirectory by lazy { File(filesDir, "tos") }
    private val tos = FirmwareSet.Part("tos.img", "TOS image",
        "tos.img · your ST's own TOS, for the games EmuTOS cannot run", setOf("tos.img", "tos.rom"), false,
        sizes = setOf(192L * 1024, 256L * 1024, 512L * 1024))
    override val firmwareSets by lazy { listOf("TOS" to FirmwareSet(tosDirectory, listOf(tos))) }

    override fun startGame(entry: DiskImageEntry, game: CatalogGame, cancelled: AtomicBoolean,
                           progress: (String) -> Unit) {
        val home = prepareHome(progress)
        val options = optionValues(game)
        if (options[StCatalog.system.id] == StCatalog.OWN_TOS) {
            check(File(tosDirectory, tos.name).isFile) { "This game starts on your TOS image: import tos.img in Menu > Machine" }
        }
        val second = game.mediaId("floppyB")?.let { id ->
            mediaFor(id, entry, libraryEntries) ?: error("The second disk this game needs is not in the library")
        }
        // In the core's drive order (st_core.cpp): drive A, drive B.
        val media = listOf(entry, second)
        val images = media.map { image -> image?.let { this.images.prepare(it, cancelled, progress).absolutePath } }
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
