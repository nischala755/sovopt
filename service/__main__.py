import uvicorn
import os


def main():
    uvicorn.run("service.app:create_app", factory=True,
                host=os.getenv("HOST", "127.0.0.1"), port=int(os.getenv("PORT", "8000")))


if __name__ == "__main__":
    main()
