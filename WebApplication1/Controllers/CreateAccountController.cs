using APIServer.Services;
using Microsoft.AspNetCore.Mvc;
using SqlKata.Execution;
using ZLogger;

namespace APIServer.Controllers;

[ApiController]
[Route("[controller]")]
public class CreateAccount : ControllerBase
{
    readonly ILogger<CreateAccount> _logger;
    readonly IGameDb _gameDb;

    public CreateAccount(ILogger<CreateAccount> logger, IGameDb gamedb)
    {
        _logger = logger;
        _gameDb = gamedb;
    }

    [HttpPost]
    public async Task<PkCreateAccountResponse> Post(PkCreateAccountRequest request)
    {
        var response = new PkCreateAccountResponse();
        response.Result = ErrorCode.None;

        var errorCode = await _gameDb.CreateAccount(request.Email, request.Password);
        if (errorCode != ErrorCode.None)
        {
            response.Result = errorCode;
            return response;
        }

        _logger.ZLogInformation($"{request.Email} Account Created");

        return response;
    }
}

public class PkCreateAccountRequest
{
    public string Email { get; set; }
    public string Password { get; set; }
}

public class PkCreateAccountResponse
{
    public ErrorCode Result { get; set; }
}
