package main

import (
	"ThroneCore/gen"
	"context"
)

func (s *server) SetSystemDNS(_ context.Context, in *gen.SetSystemDNSRequest) (*gen.EmptyResp, error) {
	return &gen.EmptyResp{}, nil
}
