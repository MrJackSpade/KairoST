package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.MachineOptionValues
import com.mrjackspade.kairo.frontend.UiText
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

/**
 * A game's System, EmuTOS or the player's TOS for its model (Kairo#112): the default follows the
 * player's TOS for the game's model, a game's own choice wins, and a game on the player's TOS
 * without it fails to start, naming the file, rather than running EmuTOS.
 */
class StSystemTest {
    @get:Rule val folder = TemporaryFolder()
    private val tos get() = folder.root
    private val options = listOf(StCatalog.machine, StCatalog.system)

    private fun values(machine: String? = null, own: String? = null, stored: String? = null) =
        MachineOptionValues.values(options, { option -> if (option === StCatalog.machine) machine else own },
            { option -> stored.takeIf { option === StCatalog.system } }) { _, values ->
            StFirmware.ownTos(values.getValue(StCatalog.machine.id), tos).isFile
        }

    private fun import(model: StTos.Model) = StFirmware.tosParts.getValue(model).let { part ->
        tos.resolve(part.name).writeBytes(ByteArray(model.bytes.toInt()))
    }

    @Test
    fun theDefaultIsThePlayersTosForTheGamesModelOnceImported() {
        assertEquals("before an import", StCatalog.EMUTOS, values().getValue(StCatalog.system.id))
        val model = StTos.forMachine(StCatalog.machine.default)
        import(model)
        assertEquals("after importing the default model's", StCatalog.OWN_TOS, values().getValue(StCatalog.system.id))
        val other = StTos.Model.entries.first { it != model }
        val otherMachine = StCatalog.machine.choices.first { StTos.forMachine(it.id) == other }.id
        assertEquals("a game on another model, whose TOS is not imported", StCatalog.EMUTOS,
            values(machine = otherMachine).getValue(StCatalog.system.id))
    }

    @Test
    fun aGamesOwnChoiceWinsOverTheAllGamesChoice() {
        import(StTos.forMachine(StCatalog.machine.default))
        assertEquals(StCatalog.EMUTOS, values(own = StCatalog.EMUTOS, stored = StCatalog.OWN_TOS).getValue(StCatalog.system.id))
        assertEquals("an all-games choice over the imported TOS", StCatalog.EMUTOS,
            values(stored = StCatalog.EMUTOS).getValue(StCatalog.system.id))
    }

    @Test
    fun aStartOnThePlayersTosWithoutItFailsNamingTheFile() {
        val model = StTos.forMachine(StCatalog.machine.default)
        val start = mapOf(StCatalog.machine.id to StCatalog.machine.default, StCatalog.system.id to StCatalog.OWN_TOS)
        assertEquals(UiText.res(R.string.own_tos_missing, StFirmware.tosParts.getValue(model).label),
            StFirmware.missingForStart(start, tos))
        assertNull("EmuTOS needs nothing", StFirmware.missingForStart(start + (StCatalog.system.id to StCatalog.EMUTOS), tos))
        import(model)
        assertNull("with the model's TOS imported", StFirmware.missingForStart(start, tos))
    }
}
