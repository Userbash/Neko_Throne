package boxmain

import (
	"context"
	"os"
	"os/user"
	"strconv"
	"time"

	"github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/experimental/deprecated"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing/service"
	"github.com/sagernet/sing/service/filemanager"

	"github.com/spf13/cobra"
)

var (
	globalCtx         context.Context
	configPaths       []string
	configDirectories []string
	workingDir        string
	disableColor      bool
)

var mainCommand = &cobra.Command{
	Use:              "sing-box",
	PersistentPreRun: preRun,
}

func init() {
	mainCommand.PersistentFlags().StringArrayVarP(&configPaths, "config", "c", nil, "set configuration file path")
	mainCommand.PersistentFlags().StringArrayVarP(&configDirectories, "config-directory", "C", nil, "set configuration directory path")
	mainCommand.PersistentFlags().StringVarP(&workingDir, "directory", "D", "", "set working directory")
	mainCommand.PersistentFlags().BoolVarP(&disableColor, "disable-color", "", false, "disable color output")
}

func preRun(cmd *cobra.Command, args []string) {
	globalCtx = context.Background()
	sudoUser := os.Getenv("SUDO_USER")

	sudoUID := 0
	sudoGID := 0
	if sudoUIDStr := os.Getenv("SUDO_UID"); sudoUIDStr != "" {
		if uid, err := strconv.Atoi(sudoUIDStr); err == nil {
			sudoUID = uid
		}
	}
	if sudoGIDStr := os.Getenv("SUDO_GID"); sudoGIDStr != "" {
		if gid, err := strconv.Atoi(sudoGIDStr); err == nil {
			sudoGID = gid
		}
	}

	if sudoUID == 0 && sudoGID == 0 && sudoUser != "" {
		sudoUserObject, err := user.Lookup(sudoUser)
		if err == nil && sudoUserObject != nil {
			if uid, err := strconv.Atoi(sudoUserObject.Uid); err == nil {
				sudoUID = uid
			}
			if gid, err := strconv.Atoi(sudoUserObject.Gid); err == nil {
				sudoGID = gid
			}
		}
	}
	if sudoUID > 0 && sudoGID > 0 {
		globalCtx = filemanager.WithDefault(globalCtx, "", "", sudoUID, sudoGID)
	}
	if disableColor {
		log.SetStdLogger(log.NewDefaultFactory(context.Background(), log.Formatter{BaseTime: time.Now(), DisableColors: true}, os.Stderr, "", nil, false).Logger())
	}
	if workingDir != "" {
		_, err := os.Stat(workingDir)
		if err != nil {
			filemanager.MkdirAll(globalCtx, workingDir, 0o777)
		}
		err = os.Chdir(workingDir)
		if err != nil {
			log.Fatal(err)
		}
	}
	if len(configPaths) == 0 && len(configDirectories) == 0 {
		configPaths = append(configPaths, "config.json")
	}
	globalCtx = service.ContextWith(globalCtx, deprecated.NewStderrManager(log.StdLogger()))
	globalCtx = box.Context(globalCtx, include.InboundRegistry(), include.OutboundRegistry(), include.EndpointRegistry(), include.DNSTransportRegistry(), include.ServiceRegistry())
}
