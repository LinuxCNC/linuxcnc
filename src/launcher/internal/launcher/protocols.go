package launcher

import (
	"github.com/sittner/linuxcnc/src/launcher/internal/protocols"
	adsprotocol "github.com/sittner/linuxcnc/src/launcher/internal/protocols/ads"
)

// initProtocols reads [PROTOCOLS] entries from the INI file and initializes
// each configured protocol. After return, all protocol HAL pins exist and
// can be wired by HAL file net/addf/setp commands.
func (l *Launcher) initProtocols() error {
	if l.ini == nil {
		return nil
	}

	// ADS protocol: [PROTOCOLS]ADS = <config.conf>
	if entries := l.ini.GetAll("PROTOCOLS", "ADS"); len(entries) > 0 {
		verbose := l.ini.Get("PROTOCOLS", "ADS_DEBUG") == "1"
		mgr := adsprotocol.NewManager(l.ini, l.logger, verbose)
		if err := mgr.Init(); err != nil {
			return err
		}
		l.protocols = append(l.protocols, mgr)
	}

	return nil
}

// startProtocols starts network listeners for all initialized protocols.
func (l *Launcher) startProtocols() error {
	for _, p := range l.protocols {
		if err := p.Start(); err != nil {
			return err
		}
	}
	return nil
}

// stopProtocols shuts down all protocol instances.
func (l *Launcher) stopProtocols() {
	for _, p := range l.protocols {
		p.Stop()
	}
}

// Ensure the import is used even if initProtocols is the only consumer.
var (
	_ protocols.Protocol = (*adsprotocol.Manager)(nil)
)
