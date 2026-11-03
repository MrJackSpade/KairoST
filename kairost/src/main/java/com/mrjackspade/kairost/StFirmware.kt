package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.FirmwareSet
import com.mrjackspade.kairo.frontend.R as FrontendR
import com.mrjackspade.kairo.frontend.UiText
import java.io.File

/** The firmware the Machine page lists, as the app's launch builds it (Kairo#125). */
object StFirmware {
    /** The player's TOS for each model, by the version its header names (StTos). */
    val tosParts = StTos.Model.entries.associateWith { model ->
        FirmwareSet.Part(model.file, UiText.res(R.string.tos_for_model, model.label),
            UiText.res(FrontendR.string.kairo_firmware_part, model.file, UiText.res(model.description)), setOf(model.file), false, sizes = setOf(model.bytes), identifies = { header -> StTos.rank(header, model) })
    }

    /** The player's TOS for the model of a game whose machine option is [machine], in [directory]. */
    fun ownTos(machine: String, directory: File): File =
        File(directory, tosParts.getValue(StTos.forMachine(machine)).name)

    /**
     * Why a game whose options are [options] cannot start on its System with the TOS in [directory]:
     * "Your TOS for its model" without that model's TOS, which never falls back to EmuTOS (Kairo#112);
     * null when it can.
     */
    fun missingForStart(options: Map<String, String>, directory: File): UiText? {
        if (options.getValue(StCatalog.system.id) != StCatalog.OWN_TOS) return null
        val machine = options.getValue(StCatalog.machine.id)
        val part = tosParts.getValue(StTos.forMachine(machine))
        return UiText.res(R.string.own_tos_missing, part.label).takeIf { !ownTos(machine, directory).isFile }
    }

    /** The TOS set, with its files in [directory]. */
    fun sets(directory: File): List<Pair<UiText, FirmwareSet>> = listOf(UiText.fixed("TOS") to FirmwareSet(directory, tosParts.values.toList()))
}
