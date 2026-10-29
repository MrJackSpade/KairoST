package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.R as FrontendR
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
import com.mrjackspade.kairo.frontend.MachineOption
import com.mrjackspade.kairo.frontend.NativeMachine
import com.mrjackspade.kairo.frontend.ShippedAssets
import com.mrjackspade.kairo.frontend.UiText
import com.mrjackspade.kairo.frontend.companionFor
import com.mrjackspade.kairo.frontend.text

import java.io.File
import java.util.concurrent.atomic.AtomicBoolean

/** KairoST: the Atari ST on Hatari, in the shared Kairo frontend. */
class MainActivity : MachineActivity<DiskImageEntry>() {
    override val identity = Identity("KAIROST", 0xffFEE902.toInt(), "KairoST", "Atari ST", "kairost",
        LibraryStrings("KAIROST", "KairoST"),
        UiText.res(R.string.credits, "KairoST", "Hatari", "Atari ST", "EmuTOS", "MrJackSpade/KairoST"),
        UiText.res(R.string.library_empty), UiText.res(FrontendR.string.kairo_library_expired),
        MachineFirstRun.Folder(UiText.res(FrontendR.string.kairo_folder_title), UiText.res(R.string.folder_description, "Atari ST", "KairoST"),
            UiText.res(FrontendR.string.kairo_folder_button)),
        UiText.res(FrontendR.string.kairo_saves_group), "MrJackSpade/KairoST")
    // The core's picture is low resolution with its borders, doubled (832 x 552); mouse motion is
    // counted in ST low-resolution pixels.
    override val machine = Machine(320, 200, 4.0 / 3.0, "state.sts")
    override val bridge by lazy { NativeMachine("kairost", listOf(StKeys.CAPS)) }
    override val keyboard = StKeys.layout
    override val maxKeyCode = StKeys.MAX_CODE
    override fun guestKey(androidKey: Int) = StKeys.fromAndroid(androidKey)
    override val joystickControls get() = StKeys.joystick
    override val controllerSpec = ControllerGuestSpec("Atari ST", StKeys.bindableCodes, { StKeys.label(it) ?: "Key $it" },
        setOf(StKeys.LEFT_SHIFT, StKeys.RIGHT_SHIFT, StKeys.CONTROL, StKeys.ALTERNATE), StKeys.joystick.zip(StKeys.joystickLabels),
        UiText.res(R.string.joystick_help), ControllerGuestSpec.STANDARD_ACTIONS, StKeys.layout)
    override val bindingsCodec = StKeys.bindings
    override val artworkDownloadsEnabled: Boolean get() = resources.getBoolean(R.bool.catalog_art_download_enabled)

    /**
     * Floppy disk images in the formats Hatari reads, hard disk images (st_machine.h Config.hard_disk),
     * and programs, which run from their folder as drive C (Config.program).
     */
    private val format = DiskImageLibrary.Format(
        kinds = listOf("st", "msa", "stx", "dim").associateWith { "fd" } +
            listOf("img", "hd").associateWith { "hd" } +
            listOf("prg", "tos", "ttp", "app", "gtp").associateWith { "prg" },
        labels = mapOf("fd" to UiText.res(FrontendR.string.kairo_floppy_disk),
            "hd" to UiText.res(FrontendR.string.kairo_hard_disk), "prg" to UiText.res(FrontendR.string.kairo_program)),
        maxBytes = mapOf("fd" to 16L * 1024 * 1024, "hd" to 4L * 1024 * 1024 * 1024, "prg" to 16L * 1024 * 1024),
        guestName = "Atari ST",
        programs = setOf("prg"))
    private val images by lazy {
        // 3: archives are read again for the hard disks in them (2: for the programs).
        DiskImageLibrary(this, format, DiskImageLibrary.Cache(mediaFormatsVersion = 3))
    }
    override fun createCatalog() = LayeredGameCatalog(this, StCatalog)
    override fun createLibrary(catalog: LayeredGameCatalog) = images

    // System lists the player's TOS images too, and imports more (StCatalog.system).
    override val machineOptions = StCatalog.options.map { option ->
        if (option.id == StCatalog.system.id) StCatalog.system(::tosChoices, ::tosActions) else option
    }
    private fun isFloppy(entry: DiskImageEntry) = format.kind(entry.displayName) == "fd"
    private fun isHardDisk(entry: DiskImageEntry) = format.kind(entry.displayName) == "hd"

    // In the core's slot order (st_core.cpp): drive A, drive B, then the hard disk, whose change restarts the machine.
    override val mediaSlots = listOf(MediaSlot<DiskImageEntry>(UiText.res(R.string.drive_a), ::isFloppy),
        MediaSlot(UiText.res(FrontendR.string.kairo_drive_b), ::isFloppy),
        MediaSlot(UiText.res(FrontendR.string.kairo_hard_disk), ::isHardDisk))

    private val tosDirectory by lazy { File(filesDir, "tos") }

    /** The player's TOS for each model, by the version its header names (StTos). */
    private val tosParts = StTos.Model.entries.associateWith { model ->
        FirmwareSet.Part(model.file, UiText.res(R.string.tos_for_model, model.label),
            UiText.res(FrontendR.string.kairo_firmware_part, model.file, UiText.res(model.description)), setOf(model.file), false, sizes = setOf(model.bytes), identifies = { header -> StTos.rank(header, model) })
    }
    override val firmwareSets by lazy { listOf(UiText.fixed("TOS") to FirmwareSet(tosDirectory, tosParts.values.toList())) }

    /** Every TOS image the player has given the app, which a game can start on by name. */
    private val tosLibrary by lazy { TosLibrary(this, tosDirectory) }

    private fun tosChoices() = tosLibrary.images().map { ChoiceOption(it.id, it.label) }

    /** Imports a TOS image into the library and picks it for the game (or all games) asking. */
    private val tosImport by lazy {
        ImportedFilePicker(this, tosLibrary.files, TOS_IMPORT_REQUEST, "tos-import", getString(R.string.tos_import), "TOS", ::toast)
    }

    private fun tosActions(select: (String) -> Unit) = tosImport.actions(select)

    override fun handleProductResult(requestCode: Int, resultCode: Int, data: Intent?): Boolean =
        tosImport.handle(requestCode, resultCode, data) || super.handleProductResult(requestCode, resultCode, data)

    /** The model rows join the library, so a game that picked one keeps it when the row changes. */
    override fun firmwareImported() = tosLibrary.mirrorRows()
    /** The player's TOS for the game's model, which "Your TOS for its model" starts. */
    override fun ownFirmwareInstalled(option: MachineOption, values: Map<String, String>): Boolean {
        check(option.id == StCatalog.system.id) { "${option.id} has no firmware of the player's" }
        val model = StTos.forMachine(values[StCatalog.machine.id] ?: StCatalog.machine.default)
        return File(tosDirectory, tosParts.getValue(model).name).isFile
    }

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
                getString(R.string.own_tos_missing, text(part.label))
            }
            options[StCatalog.system.id] = part.name
        } else if (system != StCatalog.EMUTOS) {
            val file = checkNotNull(tosLibrary.file(system)) {
                getString(R.string.tos_image_missing)
            }
            options[StCatalog.system.id] = file.relativeTo(tosDirectory).path
        }
        // A program runs from its folder, as drive C, with the drives empty; a hard disk boots with them empty.
        val program = entry.takeUnless { isFloppy(it) || isHardDisk(it) }
            ?.let { this.images.prepare(it, cancelled, progress).absolutePath }
        val second = if (!isFloppy(entry)) null else companionFor(game.mediaId("floppyB"), entry, launchEntries,
            UiText.res(FrontendR.string.kairo_second_disk_missing), ::isFloppy)
        // The program on drive A that TOS starts, "" for the automatic choice (st_core.cpp).
        options[StCatalog.PROGRAM] = if (isFloppy(entry)) StCatalog.program(game) else ""
        // In the core's order (st_core.cpp): drive A, drive B, the hard disk, then the program.
        val media = listOf(entry.takeIf(::isFloppy), second, entry.takeIf(::isHardDisk))
        val images = media.map { image -> image?.let { this.images.prepare(it, cancelled, progress).absolutePath } } + program
        check(bridge.start(tosDirectory.absolutePath, home.absolutePath, images, options)) { "Another machine is still running" }
        awaitRunning()
        mediaStarted(media)
    }

    /**
     * The core's home (st_core.cpp): "emutos", EmuTOS from assets/emutos (ShippedAssets), and
     * "hatari", Hatari's own files.
     */
    private fun prepareHome(progress: (String) -> Unit): File {
        val home = File(filesDir, "machine")
        ShippedAssets.install(this, "emutos", File(home, "emutos"), progress)
        File(home, "hatari").mkdirs()
        return home
    }

    private companion object {
        /** The picker of a TOS image for a game's System (tosActions). */
        const val TOS_IMPORT_REQUEST = FIRST_PRODUCT_REQUEST
    }
}
