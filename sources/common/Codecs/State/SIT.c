/* SIT ZX Spectrum Snapshot Codec v1.0
  ____    ____    ___ ___     ___
 / __ \  / ___\  / __` __`\  / __`\
/\ \/  \/\ \__/_/\ \/\ \/\ \/\  __/
\ \__/\_\ \_____\ \_\ \_\ \_\ \____\
 \/_/\/_/\/_____/\/_/\/_/\/_/\/____/
Copyright © 2013 Manuel Sainz de Baranda y Goñi <manuel@redcodesofware.com>
Released under the terms of the GNU General Public License v3. */

#include <Q/formats/hardware state/machine/ZX Spectrum/SIT.h>
#include <Q/hardware/machine/platform/computer/ZX Spectrum.h>
#include <Q/hardware/CPU/architecture/Z80.h>
#include <Q/functions/base/value.h>
#include <Q/keys/status.h>
#include <string.h>


static QStatus test(QSIT *object, qsize object_size)
	{
	if (object_size	!= sizeof(QSIT)) return Q_ERROR_INVALID_SIZE;

	return OK;
	}


static void decode(
	QSIT*			object,
	QZXSpectrumState*	machine,
	QZ80State*		cpu,
	quint8*			memory
)
	{
	machine->keyboard.value_uint64 = Q_UINT64(0xFFFFFFFFFFFFFFFF);
	machine->flash		       = FALSE;
	machine->ula_io.value	       = object->border_color & 7;

	Q_Z80_STATE_PC(	 cpu) = q_uint16_little_endian(object->pc);
	Q_Z80_STATE_SP(	 cpu) = q_uint16_little_endian(object->sp);
	Q_Z80_STATE_AF(	 cpu) = q_uint16_little_endian(object->af.value_uint16);
	Q_Z80_STATE_BC(	 cpu) = q_uint16_little_endian(object->bc.value_uint16);
	Q_Z80_STATE_DE(	 cpu) = q_uint16_little_endian(object->de.value_uint16);
	Q_Z80_STATE_HL(	 cpu) = q_uint16_little_endian(object->hl.value_uint16);
	Q_Z80_STATE_IX(	 cpu) = q_uint16_little_endian(object->ix.value_uint16);
	Q_Z80_STATE_IY(	 cpu) = q_uint16_little_endian(object->iy.value_uint16);
	Q_Z80_STATE_AF_( cpu) = q_uint16_little_endian(object->af_.value_uint16);
	Q_Z80_STATE_BC_( cpu) = q_uint16_little_endian(object->bc_.value_uint16);
	Q_Z80_STATE_DE_( cpu) = q_uint16_little_endian(object->de_.value_uint16);
	Q_Z80_STATE_HL_( cpu) = q_uint16_little_endian(object->hl_.value_uint16);
	Q_Z80_STATE_I(	 cpu) = object->i;
	Q_Z80_STATE_R(	 cpu) = object->r;
	Q_Z80_STATE_HALT(cpu) = FALSE;
	Q_Z80_STATE_IRQ( cpu) = FALSE;
	Q_Z80_STATE_NMI( cpu) = FALSE;
	Q_Z80_STATE_IFF1(cpu) =
	Q_Z80_STATE_IFF2(cpu) = TRUE;
	Q_Z80_STATE_EI(	 cpu) = FALSE;
	Q_Z80_STATE_IM(	 cpu) = object->im & 3;

	memcpy(memory, object->memory, 1024 * 64);
	}


static void encode(
	QSIT*			object,
	QZXSpectrumState*	machine,
	QZ80State*		cpu,
	quint8*			memory
)
	{
	object->bc.value_uint16  = q_uint16_little_endian(Q_Z80_STATE_BC(cpu));
	object->de.value_uint16  = q_uint16_little_endian(Q_Z80_STATE_DE(cpu));
	object->hl.value_uint16  = q_uint16_little_endian(Q_Z80_STATE_HL(cpu));
	object->af.value_uint16  = q_uint16_little_endian(Q_Z80_STATE_AF(cpu));
	object->ix.value_uint16  = q_uint16_little_endian(Q_Z80_STATE_IX(cpu));
	object->iy.value_uint16  = q_uint16_little_endian(Q_Z80_STATE_IY(cpu));
	object->pc		 = q_uint16_little_endian(Q_Z80_STATE_PC(cpu));
	object->sp		 = q_uint16_little_endian(Q_Z80_STATE_SP(cpu));
	object->r		 = Q_Z80_STATE_R(cpu);
	object->i		 = Q_Z80_STATE_I(cpu);
	object->bc_.value_uint16 = q_uint16_little_endian(Q_Z80_STATE_BC_(cpu));
	object->de_.value_uint16 = q_uint16_little_endian(Q_Z80_STATE_DE_(cpu));
	object->hl_.value_uint16 = q_uint16_little_endian(Q_Z80_STATE_HL_(cpu));
	object->af_.value_uint16 = q_uint16_little_endian(Q_Z80_STATE_AF_(cpu));
	object->im		 = Q_Z80_STATE_IM(cpu);
	object->border_color	 = machine->ula_io.fields.border_color;

	memcpy(object->memory, memory, 1024 * 64);
	}


/* SIT.c EOF */
