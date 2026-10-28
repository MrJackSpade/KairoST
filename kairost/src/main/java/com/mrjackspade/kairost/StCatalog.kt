package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.R as FrontendR
import com.mrjackspade.kairo.frontend.CatalogSetting
import com.mrjackspade.kairo.frontend.ChoiceOption
import com.mrjackspade.kairo.frontend.LayeredGameCatalog
import com.mrjackspade.kairo.frontend.MachineOption
import com.mrjackspade.kairo.frontend.UiText
import org.json.JSONArray

/**
 * KairoST's catalog records beyond the common fields: the machine a game starts on (`machine`,
 * by Hatari's name for it), its memory, monitor, region and system software, the second disk it
 * starts with in drive B (media role `floppyB`), and the program on drive A that TOS starts once it
 * has booted (`launch.program`), for a disk that boots to the desktop.
 */
object StCatalog : LayeredGameCatalog.Schema {
    override val platform = "atarist"
    override val catalogUrl = "https://raw.githubusercontent.com/MrJackSpade/KairoST/main/catalog/"
    override val selfHostedArt = "/MrJackSpade/KairoST/main/catalog/artwork/art/catalog/"
    override val contentId = Regex("sha256-(?:fd|prg)-v1:[0-9a-f]{64}")
    override val mediaExtensions = Regex("\\.(?:zip|7z|st|msa|stx|dim|prg|tos|ttp|app|gtp)$", RegexOption.IGNORE_CASE)

    /** The model, by Hatari's name for it. Each later model runs most of the earlier ones' games. */
    val machine = MachineOption("machine", UiText.res(FrontendR.string.kairo_option_machine), listOf(
        ChoiceOption("st", "Atari ST"),
        ChoiceOption("megast", "Atari Mega ST"),
        ChoiceOption("ste", "Atari STE"),
        ChoiceOption("megaste", "Atari Mega STE"),
        ChoiceOption("tt", "Atari TT"),
        ChoiceOption("falcon", "Atari Falcon")), "st")

    /** ST RAM in KiB. */
    val memory = MachineOption("memory", UiText.res(FrontendR.string.kairo_option_memory), listOf(
        ChoiceOption("512", UiText.res(FrontendR.string.kairo_memory_kb, 512)),
        ChoiceOption("1024", UiText.res(FrontendR.string.kairo_memory_mb, 1)),
        ChoiceOption("2048", UiText.res(FrontendR.string.kairo_memory_mb, 2)),
        ChoiceOption("4096", UiText.res(FrontendR.string.kairo_memory_mb, 4))), "1024", numeric = true)

    /** The monitor, by Hatari's name for it: color games need RGB, high resolution ones mono. */
    val monitor = MachineOption("monitor", UiText.res(R.string.option_monitor), listOf(
        ChoiceOption("rgb", UiText.res(R.string.monitor_color)),
        ChoiceOption("mono", UiText.res(R.string.monitor_mono))), "rgb")

    /**
     * EmuTOS's country (Hatari's code for it): Europe is the UK, at 50 Hz like most ST games
     * expect; USA runs at 60 Hz. A player's own TOS keeps its own.
     */
    val region = MachineOption("region", UiText.res(R.string.option_region), listOf(
        ChoiceOption("uk", UiText.res(R.string.region_europe)),
        ChoiceOption("us", UiText.res(R.string.region_usa))), "uk")

    const val EMUTOS = "emutos"
    const val OWN_TOS = "tos"

    /** The choices every game has: EmuTOS, which comes with the app, or the player's TOS for its model (StTos). */
    val fixedSystems = listOf(
        ChoiceOption(EMUTOS, "EmuTOS"),
        ChoiceOption(OWN_TOS, UiText.res(R.string.system_own_tos)))

    /**
     * The system software a game starts: [fixedSystems], or one TOS image of the player's by name
     * (TosLibrary, "tos:<hash>"), which [images] lists; [actions] add more.
     */
    fun system(images: () -> List<ChoiceOption> = { emptyList() },
               actions: (select: (String) -> Unit) -> List<com.mrjackspade.kairo.frontend.SettingsRow> = { emptyList() }) =
        MachineOption("tos", UiText.res(FrontendR.string.kairo_option_system), { fixedSystems + images() }, EMUTOS,
            accepts = { value -> value == EMUTOS || value == OWN_TOS || TosLibrary.ID.matches(value) }, actions = actions,
            ownFirmware = OWN_TOS)

    /** The System option as the catalog reads it; MainActivity lists the player's images too. */
    val system = system()

    val skipMemoryCheck = MachineOption.skipMemoryCheck()
    val options = listOf(machine, memory, monitor, region, system, skipMemoryCheck)

    /** The program setting's catalog place: `launch.program`. */
    const val PROGRAM = "program"
    const val LAUNCH = "launch"

    /**
     * A program in drive A's disk for TOS to start, by its path from the disk's top folder, such as
     * GAME.PRG or GAMES\RUN.PRG; "" for the automatic choice (core/st_machine.h floppy_program).
     */
    val programPath = Regex("""(?:[A-Za-z0-9_\-!~#$%&@^'(){}]{1,8}(?:\.[A-Za-z0-9_\-]{1,3})?\\)*""" +
        """[A-Za-z0-9_\-!~#$%&@^'(){}]{1,8}\.[A-Za-z]{1,3}""")

    /**
     * The program on drive A's disk for TOS to start once it has booted, for a disk that boots to
     * the desktop; saving nothing returns to the automatic choice, even where the catalog names one.
     */
    val program = CatalogSetting.Text(LAUNCH, PROGRAM, UiText.res(R.string.startup_program),
        UiText.res(R.string.startup_program_hint), programPath, UiText.res(R.string.startup_program_automatic),
        { it.trim().uppercase() }) { it.startsWith("sha256-fd-v1:") }

    override val settings: List<CatalogSetting> = options.map(CatalogSetting::Choice) +
        CatalogSetting.Drive("floppyB", UiText.res(R.string.drive_b)) + program
    override val fields = CatalogSetting.fields(settings)

    override fun validMedia(role: String, contentId: String) = role == "floppyB" && contentId.startsWith("sha256-fd-v1:")

    override fun validField(field: String, value: Any): Boolean =
        field in fields && CatalogSetting.validSection(settings, field, value)

    /** The game's startup program: its own, or "" for the automatic choice. */
    fun program(game: com.mrjackspade.kairo.frontend.CatalogGame): String = program.value(game)

    override fun validBindings(bindings: JSONArray) = StKeys.bindings.valid(bindings)
}
