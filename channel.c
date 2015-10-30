/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2015 Bruno Randolf (br1@einfach.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>

#include "main.h"
#include "util.h"
#include "ifctrl.h"
#include "channel.h"


static struct timeval last_channelchange;
static struct channel_list channels;

long
channel_get_remaining_dwell_time(void)
{
	if (!conf.do_change_channel)
		return LONG_MAX;

	return conf.channel_time
		- the_time.tv_sec * 1000000
		- the_time.tv_usec
		+ last_channelchange.tv_sec * 1000000
		+ last_channelchange.tv_usec;
}


bool
channel_change(int idx)
{
	if (!ifctrl_iwset_freq(conf.ifname, channels.chan[idx].freq)) {
		printlog("ERROR: could not set channel %d", channels.chan[idx].chan);
		return false;
	}
	conf.channel_idx = idx;
	last_channelchange = the_time;
	return true;
}


bool
channel_auto_change(void)
{
	int new_idx;
	bool ret = true;
	int start_idx;

	if (conf.channel_idx == -1)
		return false; /* The current channel is still unknown for some
			   * reason (mac80211 bug, busy physical interface,
			   * etc.), it will be fixed when the first packet
			   * arrives, see fixup_packet_channel().
			   *
			   * Without this return, horst would busy-loop forever
			   * (making the ui totally unresponsive) in the channel
			   * changing code below because start_idx would be -1
			   * as well. Short-circuit exiting here is quite
			   * logical though: it does not make any sense to scan
			   * channels as long as the channel module is not
			   * initialized properly. */

	if (channel_get_remaining_dwell_time() > 0)
		return false; /* too early */

	if (conf.do_change_channel) {
		start_idx = new_idx = conf.channel_idx;
		do {
			new_idx = new_idx + 1;
			if (new_idx >= channels.num_channels ||
			    new_idx >= MAX_CHANNELS ||
			    (conf.channel_max &&
			     channel_get_chan_from_idx(new_idx) > conf.channel_max))
				new_idx = 0;

			ret = channel_change(new_idx);

		/* try setting different channels in case we get errors only
		 * on some channels (e.g. ipw2200 reports channel 14 but cannot
		 * be set to use it). stop if we tried all channels */
		} while (ret != 1 && new_idx != start_idx);
	}

	return ret;
}


int
channel_get_current_chan() {
	return channel_get_chan_from_idx(conf.channel_idx);
}


bool
channel_init(void) {
	/* get available channels */
	ifctrl_iwget_freqlist(conf.if_phy, &channels);

	printf("Got %d Bands, %d Channels:\n", channels.num_bands, channels.num_channels);
	for (int i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		printf("%-3d: %d\n", channels.chan[i].chan, channels.chan[i].freq);

	conf.channel_idx = channel_find_index_from_freq(conf.if_freq);

	if (conf.channel_num_initial > 0) {
	    if (!channel_change(channel_find_index_from_chan(conf.channel_num_initial)))
	        return false;
	} else {
	    conf.channel_num_initial = channel_get_chan_from_idx(conf.channel_idx);
	}
	conf.channel_initialized = 1;
	return true;
}


int
channel_find_index_from_chan(int c)
{
	int i = -1;
	for (i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		if (channels.chan[i].chan == c)
			return i;
	return -1;
}


int
channel_find_index_from_freq(unsigned int f)
{
	int i = -1;
	for (i = 0; i < channels.num_channels && i < MAX_CHANNELS; i++)
		if (channels.chan[i].freq == f)
			return i;
	return -1;
}


int
channel_get_chan_from_idx(int i) {
	if (i >= 0 && i < channels.num_channels && i < MAX_CHANNELS)
		return channels.chan[i].chan;
	else
		return -1;
}


struct chan_freq*
channel_get_struct(int i) {
	if (i < channels.num_channels && i < MAX_CHANNELS)
		return &channels.chan[i];
	return NULL;
}


bool
channel_list_add(int chan, int freq) {
	if (channels.num_channels >=  MAX_CHANNELS)
		return false;

	channels.chan[channels.num_channels].chan = chan;
	channels.chan[channels.num_channels].freq = freq;
	channels.num_channels++;
	return true;
}

int
channel_get_num_channels() {
	return channels.num_channels;
}
